/***************************************************************************
 * 
 * Copyright (c) 2022 str2num.com, Inc. All Rights Reserved
 * $Id$ 
 * 
 **************************************************************************/
 
 
 
/**
 * @file ice_transport_channel.cpp
 * @author str2num
 * @version $Revision$ 
 * @brief 
 *  
 **/

#include "ice/ice_transport_channel.h"

#include <rtc_base/logging.h>
#include <rtc_base/time_utils.h>
#include <rtc_base/task_utils/to_queued_task.h>

#include "ice/udp_port.h"
#include "ice/ice_connection.h"

namespace ice {

const int PING_INTERVAL_DIFF = 5;
#if defined(ICE_POSIX)

void IcePingCb(EventLoop* /*el*/, TimerWatcher* /*w*/, void* data) {
    IceTransportChannel* channel = (IceTransportChannel*)data;
    channel->OnCheckAndPing();
}

IceTransportChannel::IceTransportChannel(EventLoop* el, 
        PortAllocator* allocator,
        const std::string& transport_name,
        int component) :
    el_(el),
    transport_name_(transport_name),
    component_(component),
    allocator_(allocator),
    ice_controller_(new IceController(this))
{
    RTC_LOG(LS_INFO) << "ice transport channel created, transport_name: " << transport_name_
        << ", component: " << component_;
    ping_watcher_ = el_->CreateTimer(IcePingCb, this, true);
}

#elif defined(ICE_WIN)

IceTransportChannel::IceTransportChannel(rtc::Thread* network_thread,
    PortAllocator* allocator,
    const std::string& transport_name,
    int component) :
    network_thread_(network_thread),
    transport_name_(transport_name),
    component_(component),
    allocator_(allocator),
    ice_controller_(new IceController(this))
{
    RTC_LOG(LS_INFO) << "ice transport channel created, transport_name: " << transport_name_
        << ", component: " << component_;
}

#endif

IceTransportChannel::~IceTransportChannel() {
#if defined(ICE_POSIX)
    if (ping_watcher_) {
        el_->DeleteTimer(ping_watcher_);
        ping_watcher_ = nullptr;
    }
#endif
    
    std::vector<IceConnection*> connections = ice_controller_->connections();
    for (auto conn : connections) {
        conn->Destroy();
    }

    for (auto port : ports_) {
        delete port;
    }
    ports_.clear();

    ice_controller_.reset(nullptr);

    RTC_LOG(LS_INFO) << ToString() << ": IceTransportChannel destroy";
}

void IceTransportChannel::set_ice_params(const IceParameters& ice_params) {
    RTC_LOG(LS_INFO) << "set ICE param, transport_name: " << transport_name_
        << ", component: " << component_
        << ", ufrag: " << ice_params.ice_ufrag
        << ", pwd: " << ice_params.ice_pwd;
    ice_params_ = ice_params;
}

void IceTransportChannel::set_remote_ice_params(const IceParameters& ice_params) {
    RTC_LOG(LS_INFO) << "set remote ICE param, transport_name: " << transport_name_
        << ", component: " << component_
        << ", ufrag: " << ice_params.ice_ufrag
        << ", pwd: " << ice_params.ice_pwd;
    remote_ice_params_ = ice_params;

    for (auto conn : ice_controller_->connections()) {
        conn->MaybeSetRemoteIceParams(ice_params);
    }

    SortConnectionsAndUpdateState(IceControllerEvent::REMOTE_CANDIDATE_GENERATION_CHANGE);
}

void IceTransportChannel::set_ice_role(IceRole role) {
    if (ice_role_ != role) {
        ice_role_ = role;

        for (auto port : ports_) {
            port->set_ice_role(role);
        }
    }
}

void IceTransportChannel::AddRemoteCandidate(const Candidate& candidate) {
    // ICE candidates don't need to have username and password set, but
    // the code below this (specifically, ConnectionRequest::Prepare in
    // port.cc) uses the remote candidates's username.  So, we set it
    // here.
    RTC_LOG(LS_INFO) << "add remote candidate: " << candidate.ToString();

    Candidate new_remote_candidate(candidate);
    if (!remote_ice_params_.ice_ufrag.empty()) {
        if (candidate.username.empty()) {
            new_remote_candidate.username = remote_ice_params_.ice_ufrag;
        }

        if (new_remote_candidate.username == remote_ice_params_.ice_ufrag) {
            if (candidate.password.empty()) {
                new_remote_candidate.password = remote_ice_params_.ice_pwd;
            }
        }
    }

    FinishAddingRemoteCandidate(new_remote_candidate);
}

void IceTransportChannel::GatheringCandidate() {
    if (ice_params_.ice_ufrag.empty() || ice_params_.ice_pwd.empty()) {
        RTC_LOG(LS_WARNING) << "cannot gathering candidate because ICE param is empty"
            << ", transport_name: " << transport_name_
            << ", component: " << component_
            << ", ufrag: " << ice_params_.ice_ufrag
            << ", pwd: " << ice_params_.ice_pwd;
        return;
    }
    
    auto network_list = allocator_->GetNetworks();
    if (network_list.empty()) {
        RTC_LOG(LS_WARNING) << "cannot gathering candidate because network_list is empty"
            << ", transport_name: " << transport_name_
            << ", component: " << component_;
        return;
    }

    for (auto network : network_list) {
#if defined(ICE_POSIX)
        UDPPort* port = new UDPPort(el_, transport_name_, component_, ice_params_);
#elif defined(ICE_WIN)
        UDPPort* port = new UDPPort(network_thread_, transport_name_, component_, ice_params_);
#endif
        port->SignalUnknownAddress.connect(this,
                &IceTransportChannel::OnUnknownAddress);
        
        ports_.push_back(port);

        Candidate c;
        int ret = port->CreateIceCandidate(network, allocator_->min_port(), 
                allocator_->max_port(), c);
        if (ret != 0) {
            continue;
        }

        local_candidates_.push_back(c);

        for (auto remote_candidate : remote_candidates_) {
            CreateConnection(port, remote_candidate);
        }

        SortConnectionsAndUpdateState(IceControllerEvent::NEW_CONNECTION_FROM_LOCAL_CANDIDATE);
    }

    SignalCandidateAllocateDone(this, local_candidates_);
}

void IceTransportChannel::OnUnknownAddress(UDPPort* port,
        const rtc::SocketAddress& addr,
        StunMessage* msg,
        const std::string& remote_ufrag)
{
    const StunUInt32Attribute* priority_attr = msg->GetUInt32(STUN_ATTR_PRIORITY);
    if (!priority_attr) {
        RTC_LOG(LS_WARNING) << ToString() << ": priority not found in the"
            << " binding request message, remote_addr: " << addr.ToString();
        port->SendBindingErrorResponse(msg, addr, STUN_ERROR_BAD_REQUEST,
                STUN_ERROR_REASON_BAD_REQUEST);
        return;
    }

    uint32_t remote_priority = priority_attr->value();
    Candidate remote_candidate;
    remote_candidate.component = component_;
    remote_candidate.protocol = "udp";
    remote_candidate.address = addr;
    remote_candidate.username = remote_ufrag;
    remote_candidate.password = remote_ice_params_.ice_pwd;
    remote_candidate.priority = remote_priority;
    remote_candidate.type = PRFLX_PORT_TYPE;
    
    RTC_LOG(LS_INFO) << ToString() << ": create peer reflexive candidate: "
        << remote_candidate.ToString();

    IceConnection* conn = port->CreateConnection(remote_candidate);
    if (!conn) {
        RTC_LOG(LS_WARNING) << ToString() << ": create connection from "
            << " peer reflexive candidate error, remote_addr: "
            << addr.ToString();
        port->SendBindingErrorResponse(msg, addr, STUN_ERROR_SERVER_ERROR,
                STUN_ERROR_REASON_SERVER_ERROR);
        return;
    }

    RTC_LOG(LS_INFO) << ToString() << ": create connection from "
        << " peer reflexive candidate success, remote_addr: "
        << addr.ToString();
    
    AddConnection(conn);

    conn->HandleStunBindingRequest(msg);

    SortConnectionsAndUpdateState(IceControllerEvent::NEW_CONNECTION_FROM_UNKNOWN_REMOTE_ADDRESS);
}

void IceTransportChannel::AddConnection(IceConnection* conn) {
    conn->SignalStateChange.connect(this, 
            &IceTransportChannel::OnConnectionStateChange);
    conn->SignalConnectionDestroy.connect(this,
            &IceTransportChannel::OnConnectionDestroyed);
    conn->SignalReadPacket.connect(this,
            &IceTransportChannel::OnReadPacket);
    
    had_connection_ = true;
    
    ice_controller_->AddConnection(conn);
}

void IceTransportChannel::OnReadPacket(IceConnection* /*conn*/,
        const char* buf, size_t len, int64_t ts)
{
    SignalReadPacket(this, buf, len, ts);
}

void IceTransportChannel::OnConnectionDestroyed(IceConnection* conn) {
    ice_controller_->OnConnectionDestroyed(conn);
    RTC_LOG(LS_INFO) << ToString() << ": Remove connection: " << conn
        << " with " << ice_controller_->connections().size() << " remaining";
    if (selected_connection_ == conn) {
        RTC_LOG(LS_INFO) << ToString() 
            << ": Selected connection destroyed, should select a new connection";
        SwitchSelectedConnection(nullptr);
        SortConnectionsAndUpdateState(IceControllerEvent::SELECTED_CONNECTION_DESTROYED);
    } else {
        UpdateState();
    }
}

void IceTransportChannel::OnConnectionStateChange(IceConnection* /*conn*/) {
    SortConnectionsAndUpdateState(IceControllerEvent::CONNECT_STATE_CHANGE);
}

void IceTransportChannel::SortConnectionsAndUpdateState(IceControllerEvent reason_to_sort) {
    MaybeSwitchSelectedConnection(ice_controller_->SortAndSwitchConnection(), reason_to_sort);
    
    UpdateState();
    
    MaybeStartPinging();
}

void IceTransportChannel::set_writable(bool writable) {
    if (writable_ == writable) {
        return;
    }
    
    if (writable) {
        has_been_connection_ = true;
    }

    writable_ = writable;
    RTC_LOG(LS_INFO) << ToString() << ": Change writable to " << writable_;
    SignalWritableState(this);
}

void IceTransportChannel::set_receiving(bool receiving) {
    if (receiving_ == receiving) {
        return;
    }

    receiving_ = receiving;
    RTC_LOG(LS_INFO) << ToString() << ": Change receiving to " << receiving_;
    SignalReceivingState(this);
}

void IceTransportChannel::UpdateState() {
    bool writable = selected_connection_ && selected_connection_->writable();
    set_writable(writable);

    bool receiving = false;
    for (auto conn : ice_controller_->connections()) {
        if (conn->receiving()) {
            receiving = true;
            break;
        }
    }
    set_receiving(receiving);

    IceTransportState state = ComputeIceTransportState();
    if (state != state_) {
        state_ = state;
        SignalIceStateChanged(this);
    }
}

IceTransportState IceTransportChannel::ComputeIceTransportState() {
    bool has_connection = false;
    for (auto conn : ice_controller_->connections()) {
        if (conn->active()) {
            has_connection = true;
            break;
        }
    }

    if (had_connection_ && !has_connection) {
        return IceTransportState::kFailed;
    }

    if (has_been_connection_ && !writable()) {
        return IceTransportState::kDisconnected;
    }

    if (!had_connection_ && !has_connection) {
        return IceTransportState::kNew;
    }
    
    if (has_connection && !writable()) {
        return IceTransportState::kChecking;
    }

    return IceTransportState::kConnected;
}

bool IceTransportChannel::CreateConnections(const Candidate& remote_candidate) {
    // If we've already seen the new remote candidate (in the current candidate
    // generation), then we shouldn't try creating connections for it.
    // We either already have a connection for it, or we previously created one
    // and then later pruned it. If we don't return, the channel will again
    // re-create any connections that were previously pruned, which will then
    // immediately be re-pruned, churning the network for no purpose.
    // This only applies to candidates received over signaling (i.e. origin_port
    // is NULL).
    if (IsDuplicateRemoteCandidate(remote_candidate)) {
        // return true to indicate success, without creating any new connections.
        return true;
    }

    bool created = false;
    std::vector<UDPPort*>::reverse_iterator it;
    for (it = ports_.rbegin(); it != ports_.rend(); ++it) {
        if (CreateConnection(*it, remote_candidate)) {
            created = true;
        }
    }

    // Remember this remote candidate so that we can add it to future ports.
    RememberRemoteCandidate(remote_candidate);

    return created;
}

bool IceTransportChannel::CreateConnection(UDPPort* port, const Candidate& remote_candidate) {
    IceConnection* connection = port->GetConnection(remote_candidate.address);
    if (connection == nullptr) {
        IceConnection* connection = port->CreateConnection(remote_candidate);
        if (!connection) {
            return false;
        }
        AddConnection(connection);
        RTC_LOG(LS_INFO) << ToString() << ": Created connection, total: " << ice_controller_->connections().size();
        return true;
    }

    // No new connection was created.
    // It is not legal to try to change any of the parameters of an existing
    // connection; however, the other side can send a duplicate candidate.
    if (!remote_candidate.IsEquivalent(connection->remote_candidate())) {
        RTC_LOG(INFO) << "Attempt to change a remote candidate."
            " Existing remote candidate: "
            << connection->remote_candidate().ToString()
            << "New remote candidate: "
            << remote_candidate.ToString();
    }
    return false;
}

bool IceTransportChannel::IsDuplicateRemoteCandidate(const Candidate& candidate) {
    for (size_t i = 0; i < remote_candidates_.size(); ++i) {
        if (remote_candidates_[i].IsEquivalent(candidate)) {
            return true;
        }
    }
    return false;
}

void IceTransportChannel::RememberRemoteCandidate(const Candidate& remote_candidate) {
    // Make sure this candidate is not a duplicate.
    if (IsDuplicateRemoteCandidate(remote_candidate)) {
        RTC_LOG(INFO) << "Duplicate candidate: " << remote_candidate.ToString();
        return;
    }

    // Try this candidate for all future ports.
    remote_candidates_.push_back(remote_candidate);
}

void IceTransportChannel::FinishAddingRemoteCandidate(const Candidate& new_remote_candidate) {
    // If this candidate matches what was thought to be a peer reflexive
    // candidate, we need to update the candidate priority/etc.
    for (IceConnection* conn : ice_controller_->connections()) {
        conn->MaybeUpdatePeerReflexiveCandidate(new_remote_candidate);
    }

    // Create connections to this remote candidate.
    CreateConnections(new_remote_candidate);

    // Resort the connections list, which may have new elements.
    SortConnectionsAndUpdateState(
        IceControllerEvent::NEW_CONNECTION_FROM_REMOTE_CANDIDATE);
}

void IceTransportChannel::MaybeSwitchSelectedConnection(IceConnection* conn, IceControllerEvent reason) {
    if (!conn) {
        return;
    }

    RTC_LOG(LS_INFO) << "Switching selected connection due to: " << reason.ToString();
    SwitchSelectedConnection(conn);
}

void IceTransportChannel::SwitchSelectedConnection(IceConnection* conn) { 
    IceConnection* old_selected_connection = selected_connection_;
    selected_connection_ = conn;
    if (old_selected_connection) {
        old_selected_connection->set_selected(false);
        RTC_LOG(LS_INFO) << ToString() << ": Previous connection: "
            << old_selected_connection->ToString();
    }

    if (selected_connection_) {
        RTC_LOG(LS_INFO) << ToString() << ": New selected connection: "
            << conn->ToString();

        selected_connection_ = conn;
        selected_connection_->set_selected(true);
        ice_controller_->SetSelectedConnection(selected_connection_);
    } else {
        RTC_LOG(LS_INFO) << ToString() << ": No connection selected";
    }
}

void IceTransportChannel::MaybeStartPinging() {
    if (start_pinging_) {
        return;
    }

    if (ice_controller_->HasPingableConnection()) {
        RTC_LOG(LS_INFO) << ToString() << ": Have a pingable connection "
            << "for the first time, starting to ping";

#if defined(ICE_POSIX)
        // 启动定时器
        el_->StartTimer(ping_watcher_, cur_ping_interval_ * 1000);
#elif defined(ICE_WIN)
        network_thread_->PostTask(webrtc::ToQueuedTask(task_safety_, [=]() {
            OnCheckAndPing();
        }));
#endif
        start_pinging_ = true;
    }
}

void IceTransportChannel::OnCheckAndPing() {
    UpdateConnectionStates();

    auto result = ice_controller_->SelectConnectionToPing(
            last_ping_sent_ms_ - PING_INTERVAL_DIFF);
        
    if (result.conn) {
        IceConnection* conn = (IceConnection*)result.conn;
        PingConnection(conn);
        ice_controller_->MarkConnectionPinged(conn);
    }

    if (cur_ping_interval_ != result.ping_interval) {
        cur_ping_interval_ = result.ping_interval;

#if defined(ICE_POSIX)
        el_->StopTimer(ping_watcher_);
        el_->StartTimer(ping_watcher_, cur_ping_interval_ * 1000);
#endif
    }
#if defined(ICE_WIN)
        network_thread_->PostDelayedTask(
            webrtc::ToQueuedTask(task_safety_, [=]() { OnCheckAndPing(); }), cur_ping_interval_);
#endif
}

void IceTransportChannel::UpdateConnectionStates() {
    std::vector<IceConnection*> connections = ice_controller_->connections();
    int64_t now = rtc::TimeMillis();
    for (auto conn : connections) {
        conn->UpdateState(now);
    }
}

void IceTransportChannel::PingConnection(IceConnection* conn) {
    last_ping_sent_ms_ = rtc::TimeMillis();
    conn->Ping(last_ping_sent_ms_);
}

int IceTransportChannel::SendPacket(const char* data, size_t len) {
    if (!ice_controller_->ReadyToSend(selected_connection_)) {
        RTC_LOG(LS_WARNING) << ToString() << ": Selected connection not ready to send.";
        return -1;
    }

    int sent = selected_connection_->SendPacket(data, len);
    if (sent <= 0) {
        RTC_LOG(LS_WARNING) << ToString() << ": Selected connection send failed.";
    }

    return sent;
}

std::string IceTransportChannel::ToString() {
    std::stringstream ss;
    ss << "Channel[" << this << ":" << transport_name_ << ":" << component_
        << "]";
    return ss.str();
}

} // namespace ice


