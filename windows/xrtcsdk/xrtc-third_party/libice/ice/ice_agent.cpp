/***************************************************************************
 * 
 * Copyright (c) 2022 ice.com, Inc. All Rights Reserved
 * $Id$ 
 * 
 **************************************************************************/
 
 
 
/**
 * @file ice_agent.cpp
 * @version $Revision$ 
 * @brief 
 *  
 **/

#include "ice/ice_agent.h"

#include <algorithm>

#include <rtc_base/logging.h>
#include <rtc_base/ssl_fingerprint.h>
#include <rtc_base/task_utils/to_queued_task.h>

namespace ice {

#if defined(ICE_POSIX)

void DestoryCb(EventLoop* el, TimerWatcher* w, void* data) {
    (void)el;
    (void)w;

    IceAgent* agent = (IceAgent*)data;
    delete agent;
}

IceAgent::IceAgent(EventLoop* el, PortAllocator* allocator) :
    el_(el), allocator_(allocator) {}

void IceAgent::Destroy() {
    if (!destroy_watcher_) {
        destroy_watcher_ = el_->CreateTimer(DestoryCb, (void*)this, false);
        el_->StartTimer(destroy_watcher_, 1000);
    }
}

#elif defined(ICE_WIN)
IceAgent::IceAgent(rtc::Thread* network_thread, PortAllocator* allocator) :
    network_thread_(network_thread), allocator_(allocator) 
{
    if (!network_thread) {
        inner_network_thread_ = rtc::Thread::CreateWithSocketServer();
        inner_network_thread_->SetName("ice_network_thread", nullptr);
        inner_network_thread_->Start();
        network_thread_ = inner_network_thread_.get();
    }
}

void IceAgent::Destroy() {
    network_thread_->PostTask(webrtc::ToQueuedTask([=]() {
        delete this;
    }));
}
#endif

IceAgent::~IceAgent() {
#if defined(ICE_POSIX)
    if (destroy_watcher_) {
        el_->DeleteTimer(destroy_watcher_);
        destroy_watcher_ = nullptr;
    }
#elif defined(ICE_WIN)
    if (inner_network_thread_) {
        inner_network_thread_->Stop();
    }
#endif

#if defined(USE_DTLS)
    for (auto dtls_srtp : dtls_srtp_transports_) {
        delete dtls_srtp;
    }

    dtls_srtp_transports_.clear();

    for (auto dtls : dtls_transports_) {
        delete dtls;
    }

    dtls_transports_.clear();
#endif

    for (auto channel : ice_transports_) {
        delete channel;
    }

    ice_transports_.clear();
}

IceTransportChannel* IceAgent::GetIceTransport(const std::string& transport_name,
        int component)
{
    auto iter = GetIceTransportInternal(transport_name, component);
    return iter == ice_transports_.end() ? nullptr : *iter;
}

bool IceAgent::CreateIceTransport(const std::string& transport_name,
        int component)
{
#if defined(ICE_WIN)
    if (!network_thread_->IsCurrent()) {
        return network_thread_->Invoke<bool>(RTC_FROM_HERE, [=] { 
            return CreateIceTransport(transport_name, component);
        });
    }
#endif

    if (GetIceTransport(transport_name, component)) {
        return true;
    }
   
#if defined(ICE_POSIX)
    auto channel = new IceTransportChannel(el_, allocator_, transport_name, component);
#elif defined(ICE_WIN)
    auto channel = new IceTransportChannel(network_thread_, allocator_, transport_name, component);
#endif

    channel->SignalCandidateAllocateDone.connect(this, 
            &IceAgent::OnCandidateAllocateDone);
    channel->SignalReceivingState.connect(this,
            &IceAgent::OnIceReceivingState);
    channel->SignalWritableState.connect(this,
            &IceAgent::OnIceWritableState);
    channel->SignalIceStateChanged.connect(this,
            &IceAgent::OnIceStateChanged);
    channel->SignalReadPacket.connect(this,
            &IceAgent::OnReadPacket);

    ice_transports_.push_back(channel);

    return true;
}

void IceAgent::DestroyIceTransport(const std::string& transport_name, int component) {
#if defined(ICE_WIN)
    if (!network_thread_->IsCurrent()) {
        network_thread_->Invoke<void>(RTC_FROM_HERE, [=] {
            DestroyIceTransport(transport_name, component);
        });
        return;
    }
#endif

    auto iter = ice_transports_.begin();
    for (; iter != ice_transports_.end(); ++iter) {
        if ((*iter)->transport_name() == transport_name && (*iter)->component() == component) {
            ice_transports_.erase(iter);
            delete *iter;
            break;
        }
    }
}

void IceAgent::DestroyAllIceTransport() {
#if defined(ICE_WIN)
    if (!network_thread_->IsCurrent()) {
        network_thread_->Invoke<void>(RTC_FROM_HERE, [=] {
            DestroyAllIceTransport();
        });
        return;
    }
#endif

    auto iter = ice_transports_.begin();
    for (; iter != ice_transports_.end(); ++iter) {
            delete *iter;
    }

    ice_transports_.clear();
}

#if defined(USE_DTLS)

bool IceAgent::CreateDtlsTransport(const std::string& transport_name, int component) {
#if defined(ICE_WIN)
    if (!network_thread_->IsCurrent()) {
        return network_thread_->Invoke<bool>(RTC_FROM_HERE, [=] {
            return CreateDtlsTransport(transport_name, component);
        });
    }
#endif

    CreateIceTransport(transport_name, component);
    auto ice_channel = GetIceTransport(transport_name, component);

    if (!ice_channel) {
        return false;
    }

    ice_channel->SignalReadPacket.disconnect(this);

    dtls_ = true;

    auto dtls = new DtlsTransportChannel(ice_channel);
    dtls->SignalReceivingState.connect(this, &IceAgent::OnDtlsReceivingState);
    dtls->SignalWritableState.connect(this, &IceAgent::OnDtlsWritableState);
    dtls->SignalDtlsState.connect(this, &IceAgent::OnDtlsState);
    dtls_transports_.push_back(dtls);

    return true;
}

void IceAgent::DestroyDtlsTransport(const std::string& transport_name, int component) {
#if defined(ICE_WIN)
    if (!network_thread_->IsCurrent()) {
        network_thread_->Invoke<void>(RTC_FROM_HERE, [=] {
            DestroyDtlsTransport(transport_name, component);
        });
        return;
    }
#endif

    auto iter = dtls_transports_.begin();
    for (; iter != dtls_transports_.end(); ++iter) {
        if ((*iter)->transport_name() == transport_name && (*iter)->component() == component) {
            dtls_transports_.erase(iter);
            delete* iter;
            break;
        }
    }

    auto srtp_iter = dtls_srtp_transports_.begin();
    for (; srtp_iter != dtls_srtp_transports_.end(); ++srtp_iter) {
        if ((*srtp_iter)->transport_name() == transport_name) {
            dtls_srtp_transports_.erase(srtp_iter);
            delete* srtp_iter;
            break;
        }
    }
}

void IceAgent::DestroyAllDtlsTransport() {
#if defined(ICE_WIN)
    if (!network_thread_->IsCurrent()) {
        network_thread_->Invoke<void>(RTC_FROM_HERE, [=] {
            DestroyAllDtlsTransport();
        });
        return;
    }
#endif

    auto iter = dtls_transports_.begin();
    for (; iter != dtls_transports_.end(); ++iter) {
        delete *iter;
    }

    dtls_transports_.clear();

    auto srtp_iter = dtls_srtp_transports_.begin();
    for (; srtp_iter != dtls_srtp_transports_.end(); ++iter) {
        delete *srtp_iter;
    }

    dtls_srtp_transports_.clear();
}

void IceAgent::OnDtlsReceivingState(DtlsTransportChannel*) {
    UpdateDtlsState();
}

void IceAgent::OnDtlsWritableState(DtlsTransportChannel*) {
    UpdateDtlsState();
}

void IceAgent::OnDtlsState(DtlsTransportChannel*, DtlsTransportState) {
    UpdateDtlsState();
}

void IceAgent::OnUnSrtpPacket(DtlsSrtpTransport* dtls_srtp, rtc::CopyOnWriteBuffer* buf, int64_t ts) {
    SignalUnSrtpPacket(this, dtls_srtp->transport_name(), (const char*)buf->data(), buf->size(), ts);
}

void IceAgent::OnUnSrtcpPacket(DtlsSrtpTransport* dtls_srtp, rtc::CopyOnWriteBuffer* buf, int64_t ts) {
    SignalUnSrtcpPacket(this, dtls_srtp->transport_name(), (const char*)buf->data(), buf->size(), ts);
}

void IceAgent::UpdateDtlsState() {
    IceDtlsTransportState ice_dtls_state = IceDtlsTransportState::kNew;

    std::map<DtlsTransportState, int> dtls_state_counts;
    std::map<IceTransportState, int> ice_state_counts;
    auto iter = dtls_transports_.begin();
    for (; iter != dtls_transports_.end(); ++iter) {
        dtls_state_counts[(*iter)->dtls_state()]++;
        ice_state_counts[(*iter)->ice_channel()->state()]++;
    }

    int total_connected = ice_state_counts[IceTransportState::kConnected] +
        dtls_state_counts[DtlsTransportState::kConnected];
    int total_dtls_connecting = dtls_state_counts[DtlsTransportState::kConnecting];
    int total_failed = ice_state_counts[IceTransportState::kFailed] +
        dtls_state_counts[DtlsTransportState::kFailed];
    int total_closed = ice_state_counts[IceTransportState::kClosed] +
        dtls_state_counts[DtlsTransportState::kClosed];
    int total_new = ice_state_counts[IceTransportState::kNew] +
        dtls_state_counts[DtlsTransportState::kNew];
    int total_ice_checking = ice_state_counts[IceTransportState::kChecking];
    int total_ice_disconnected = ice_state_counts[IceTransportState::kDisconnected];
    int total_ice_completed = ice_state_counts[IceTransportState::kCompleted];
    int total_transports = (int)dtls_transports_.size() * 2;

    if (total_failed > 0) {
        ice_dtls_state = IceDtlsTransportState::kFailed;
    }
    else if (total_ice_disconnected > 0) {
        ice_dtls_state = IceDtlsTransportState::kDisconnected;
    }
    else if (total_new + total_closed == total_transports) {
        ice_dtls_state = IceDtlsTransportState::kNew;
    }
    else if (total_ice_checking + total_dtls_connecting + total_new > 0) {
        ice_dtls_state = IceDtlsTransportState::kConnecting;
    }
    else if (total_connected + total_ice_completed + total_closed == total_transports) {
        ice_dtls_state = IceDtlsTransportState::kConnected;
    }

    if (ice_dtls_state_ != ice_dtls_state) {
        ice_dtls_state_ = ice_dtls_state;
        SignalIceDtlsState(this, ice_dtls_state);
    }
}

DtlsTransportChannel* IceAgent::GetDtlsTransport(const std::string& transport_name,
    int component)
{
    auto iter = GetDtlsTransportInternal(transport_name, component);
    return iter == dtls_transports_.end() ? nullptr : *iter;
}

DtlsSrtpTransport* IceAgent::GetDtlsSrtpTransport(const std::string& transport_name) {
    auto iter = GetDtlsSrtpTransportInternal(transport_name);
    return iter == dtls_srtp_transports_.end() ? nullptr : *iter;
}

Fingerprint IceAgent::CreateFingerprintFromCertificate(Certificate* cert) {
    Fingerprint fp;
    auto ssl_fp = rtc::SSLFingerprint::CreateFromCertificate(*cert->cert());
    if (ssl_fp) {
        fp.alg = ssl_fp->algorithm;
        fp.fingerprint = ssl_fp->GetRfc4572Fingerprint();
    }

    return fp;
}

int IceAgent::SendSrtp(const std::string& transport_name, const char* data, size_t len) {
#if defined(ICE_WIN)
    if (!network_thread_->IsCurrent()) {
        return network_thread_->Invoke<int>(RTC_FROM_HERE, [=] {
            return SendSrtp(transport_name, data, len);
            });
    }
#endif

    auto dtls_srtp = GetDtlsSrtpTransport(transport_name);
    if (!dtls_srtp) {
        return -1;
    }

    return dtls_srtp->SendSrtp(data, len);
}

int IceAgent::SendSrtcp(const std::string& transport_name, const char* data, size_t len) {
#if defined(ICE_WIN)
    if (!network_thread_->IsCurrent()) {
        return network_thread_->Invoke<int>(RTC_FROM_HERE, [=] {
            return SendSrtcp(transport_name, data, len);
            });
    }
#endif

    auto dtls_srtp = GetDtlsSrtpTransport(transport_name);
    if (!dtls_srtp) {
        return -1;
    }

    return dtls_srtp->SendSrtcp(data, len);
}

std::vector<DtlsTransportChannel*>::iterator IceAgent::GetDtlsTransportInternal(
    const std::string& transport_name,
    int component) {
    return std::find_if(dtls_transports_.begin(), dtls_transports_.end(),
        [transport_name, component](DtlsTransportChannel* channel) {
            return transport_name == channel->transport_name() &&
                component == channel->component();
        });
}

std::vector<DtlsSrtpTransport*>::iterator IceAgent::GetDtlsSrtpTransportInternal(
    const std::string& transport_name) {
    return std::find_if(dtls_srtp_transports_.begin(), dtls_srtp_transports_.end(),
        [transport_name](DtlsSrtpTransport* channel) {
            return transport_name == channel->transport_name();
        });
}

void IceAgent::SetDtlsRole(DtlsRole role) {
#if defined(ICE_WIN)
    if (!network_thread_->IsCurrent()) {
        network_thread_->Invoke<void>(RTC_FROM_HERE, [=] {
            SetDtlsRole(role);
            });
        return;
    }
#endif

    for (auto dtls : dtls_transports_) {
        dtls->SetDtlsRole((rtc::SSLRole)role);
    }
}

bool IceAgent::SetRemoteFingerprint(const std::string& alg, const std::string& fingerprint) {
#if defined(ICE_WIN)
    if (!network_thread_->IsCurrent()) {
        return network_thread_->Invoke<bool>(RTC_FROM_HERE, [=] {
            return SetRemoteFingerprint(alg, fingerprint);
            });
    }
#endif

    auto ssl_fingerprint = rtc::SSLFingerprint::CreateFromRfc4572(alg, fingerprint);

    for (auto dtls : dtls_transports_) {
        if (!dtls->SetRemoteFingerprint(alg, ssl_fingerprint->digest.data(), ssl_fingerprint->digest.size())) {
            return false;
        }
    }

    return true;
}

void IceAgent::UseDtlsSrtp(const std::string& transport_name) {
#if defined(ICE_WIN)
    if (!network_thread_->IsCurrent()) {
        network_thread_->Invoke<void>(RTC_FROM_HERE, [=] {
            UseDtlsSrtp(transport_name);
            });
        return;
    }
#endif
    dtls_srtp_ = true;

    auto dtls_srtp = GetDtlsSrtpTransport(transport_name);
    if (dtls_srtp) {
        return;
    }

    dtls_srtp = new DtlsSrtpTransport(transport_name);
    dtls_srtp_transports_.push_back(dtls_srtp);

    auto dtls = GetDtlsTransport(transport_name, TransportComponent::kRtp);
    auto dtls_rtcp = GetDtlsTransport(transport_name, TransportComponent::kRtcp);
    dtls_srtp->SetDtlsTransports(dtls, dtls_rtcp);
    dtls_srtp->SignalUnSrtpPacketReceived.connect(this, &IceAgent::OnUnSrtpPacket);
    dtls_srtp->SignalUnSrtcpPacketReceived.connect(this, &IceAgent::OnUnSrtcpPacket);
}

void IceAgent::SetLocalCertificate(Certificate* cert) {
#if defined(ICE_WIN)
    if (!network_thread_->IsCurrent()) {
        network_thread_->Invoke<void>(RTC_FROM_HERE, [=] {
            SetLocalCertificate(cert);
            });
        return;
    }
#endif

    for (auto dtls : dtls_transports_) {
        dtls->SetLocalCertificate(cert->cert());
    }
}

#endif

void IceAgent::OnCandidateAllocateDone(IceTransportChannel* channel,
            const std::vector<Candidate>& candidates)
{
    SignalCandidateAllocateDone(this, channel->transport_name(),
            channel->component(), candidates);
}

void IceAgent::OnIceReceivingState(IceTransportChannel*) {
    UpdateState();
}

void IceAgent::OnIceWritableState(IceTransportChannel*) {
    UpdateState();
}

void IceAgent::OnIceStateChanged(IceTransportChannel*) {
    UpdateState();
}

void IceAgent::OnReadPacket(IceTransportChannel* channel, const char* data, size_t len, int64_t ts) {
    SignalReadPacket(this, channel->transport_name(), channel->component(), data, len, ts);
}

void IceAgent::UpdateState() {
    IceTransportState ice_state = IceTransportState::kNew;
    std::map<IceTransportState, int> ice_state_counts;
    for (auto channel : ice_transports_) {
        ice_state_counts[channel->state()]++;
    }

    int total_ice_new = ice_state_counts[IceTransportState::kNew];
    int total_ice_checking = ice_state_counts[IceTransportState::kChecking];
    int total_ice_connected = ice_state_counts[IceTransportState::kConnected];
    int total_ice_completed = ice_state_counts[IceTransportState::kCompleted];
    int total_ice_failed = ice_state_counts[IceTransportState::kFailed];
    int total_ice_disconnected = ice_state_counts[IceTransportState::kDisconnected];
    int total_ice_closed = ice_state_counts[IceTransportState::kClosed];
    int total_ice = (int)ice_transports_.size();

    if (total_ice_failed > 0) {
        ice_state = IceTransportState::kFailed;
    } else if (total_ice_disconnected > 0) {
        ice_state = IceTransportState::kDisconnected;
    } else if (total_ice_new + total_ice_closed == total_ice) {
        ice_state = IceTransportState::kNew;
    } else if (total_ice_new + total_ice_checking > 0) {
        ice_state = IceTransportState::kChecking;
    } else if (total_ice_completed + total_ice_closed == total_ice) {
        ice_state = IceTransportState::kCompleted;
    } else if (total_ice_connected + total_ice_completed + total_ice_closed == total_ice) {
        ice_state = IceTransportState::kConnected;
    }

    if (ice_state_ != ice_state) {
        // 为了保证不跳过k_connected状态
        if (ice_state_ == IceTransportState::kChecking 
                && ice_state == IceTransportState::kCompleted)
        {
            SignalIceState(this, IceTransportState::kConnected);
#if defined(USE_DTLS)
            if (dtls_) {
                UpdateDtlsState();
            }
#endif
        }

        ice_state_ = ice_state;
        SignalIceState(this, ice_state_);

#if defined(USE_DTLS)
        if (dtls_) {
            UpdateDtlsState();
        }
#endif
    }
}

std::vector<IceTransportChannel*>::iterator IceAgent::GetIceTransportInternal(
        const std::string& transport_name,
        int component)
{
    return std::find_if(ice_transports_.begin(), ice_transports_.end(), 
            [transport_name, component](IceTransportChannel* channel) {
                return transport_name == channel->transport_name() &&
                    component == channel->component();
            });
}

void IceAgent::SetIceParams(const IceParameters& ice_params) {
#if defined(ICE_WIN)
    if (!network_thread_->IsCurrent()) {
        network_thread_->Invoke<void>(RTC_FROM_HERE, [=] { SetIceParams(ice_params); });
        return;
    }
#endif

    for (auto channel : ice_transports_) {
        channel->set_ice_params(ice_params);
    }
}

void IceAgent::SetIceParams(const std::string& transport_name,
        int component,
        const IceParameters& ice_params)
{
#if defined(ICE_WIN)
    if (!network_thread_->IsCurrent()) {
        network_thread_->Invoke<void>(RTC_FROM_HERE, [=] { 
            SetIceParams(transport_name, component, ice_params); 
        });
        return;
    }
#endif

    auto channel = GetIceTransport(transport_name, component);
    if (channel) {
        channel->set_ice_params(ice_params);
    }
}

void IceAgent::SetRemoteIceParams(const IceParameters& ice_params) {
#if defined(ICE_WIN)
    if (!network_thread_->IsCurrent()) {
        network_thread_->Invoke<void>(RTC_FROM_HERE, [=] { SetRemoteIceParams(ice_params); });
        return;
    }
#endif

    for (auto channel : ice_transports_) {
        channel->set_remote_ice_params(ice_params);
    }
}

void IceAgent::SetRemoteIceParams(const std::string& transport_name,
        int component,
        const IceParameters& ice_params)
{
#if defined(ICE_WIN)
    if (!network_thread_->IsCurrent()) {
        network_thread_->Invoke<void>(RTC_FROM_HERE, [=] { 
            SetRemoteIceParams(transport_name, component, ice_params); 
        });
        return;
    }
#endif

    auto channel = GetIceTransport(transport_name, component);
    if (channel) {
        channel->set_remote_ice_params(ice_params);
    }
}

void IceAgent::SetIceRole(IceRole role) {
#if defined(ICE_WIN)
    if (!network_thread_->IsCurrent()) {
        network_thread_->Invoke<void>(RTC_FROM_HERE, [=] {
            SetIceRole(role);
        });
        return;
    }
#endif
    
    for (auto channel : ice_transports_) {
        channel->set_ice_role(role);
    }
}

void IceAgent::SetIceRole(const std::string& transport_name, int component, IceRole role) {
#if defined(ICE_WIN)
    if (!network_thread_->IsCurrent()) {
        network_thread_->Invoke<void>(RTC_FROM_HERE, [=] {
            SetIceRole(transport_name, component, role);
        });
        return;
    }
#endif

    auto channel = GetIceTransport(transport_name, component);
    if (channel) {
        channel->set_ice_role(role);
    }
}

void IceAgent::AddRemoteCandidate(const std::string& host, int port, const IceParameters& ice_params)
{
    Candidate remote_candidate;
    remote_candidate.component = 1;
    remote_candidate.protocol = "udp";
    remote_candidate.address = rtc::SocketAddress(host, port);
    remote_candidate.port = port;
    remote_candidate.priority = remote_candidate.GetPriority(ice::ICE_TYPE_PREFERENCE_HOST, 0, 0);
    remote_candidate.username = ice_params.ice_ufrag;
    remote_candidate.password = ice_params.ice_pwd;
    remote_candidate.type = LOCAL_PORT_TYPE;
    remote_candidate.foundation = Candidate::ComputeFoundation(remote_candidate.type, 
        remote_candidate.protocol, "", remote_candidate.address);
    AddRemoteCandidate(remote_candidate);
}

void IceAgent::AddRemoteCandidate(const std::string& transport_name, int component,
    const std::string& host, int port, const IceParameters& ice_params) 
{
    Candidate remote_candidate;
    remote_candidate.component = 1;
    remote_candidate.protocol = "udp";
    remote_candidate.address = rtc::SocketAddress(host, port);
    remote_candidate.port = port;
    remote_candidate.priority = remote_candidate.GetPriority(ice::ICE_TYPE_PREFERENCE_HOST, 0, 0);
    remote_candidate.username = ice_params.ice_ufrag;
    remote_candidate.password = ice_params.ice_pwd;
    remote_candidate.type = LOCAL_PORT_TYPE;
    remote_candidate.foundation = Candidate::ComputeFoundation(remote_candidate.type,
        remote_candidate.protocol, "", remote_candidate.address);
    AddRemoteCandidate(transport_name, component, remote_candidate);
}

void IceAgent::AddRemoteCandidate(const Candidate& candidate) {
#if defined(ICE_WIN)
    if (!network_thread_->IsCurrent()) {
        network_thread_->Invoke<void>(RTC_FROM_HERE, [=] {
            AddRemoteCandidate(candidate);
        });
        return;
    }
#endif

    for (auto channel : ice_transports_) {
        channel->AddRemoteCandidate(candidate);
    }
}

void IceAgent::AddRemoteCandidate(const std::string& transport_name, int component, const Candidate& candidate) {
#if defined(ICE_WIN)
    if (!network_thread_->IsCurrent()) {
        network_thread_->Invoke<void>(RTC_FROM_HERE, [=] {
            AddRemoteCandidate(transport_name, component, candidate);
        });
        return;
    }
#endif

    auto channel = GetIceTransport(transport_name, component);
    if (channel) {
        channel->AddRemoteCandidate(candidate);
    }
}

void IceAgent::GatheringCandidate() {
#if defined(ICE_WIN)
    if (!network_thread_->IsCurrent()) {
        network_thread_->Invoke<void>(RTC_FROM_HERE, [=] {
            GatheringCandidate();
        });
        return;
    }
#endif

    for (auto channel : ice_transports_) {
        channel->GatheringCandidate();
    }
}

int IceAgent::SendPacket(const std::string& transport_name, int component, const char* data, size_t len)
{
#if defined(ICE_WIN)
    if (!network_thread_->IsCurrent()) {
        return network_thread_->Invoke<int>(RTC_FROM_HERE, [=] {
            return SendPacket(transport_name, component, data, len);
        });
    }
#endif

    auto channel = GetIceTransport(transport_name, component);
    if (!channel) {
        return -1;
    }

    return channel->SendPacket(data, len);
}

} // namespace ice


