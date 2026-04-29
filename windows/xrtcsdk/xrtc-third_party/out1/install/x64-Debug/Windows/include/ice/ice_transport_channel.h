/***************************************************************************
 * 
 * Copyright (c) 2022 str2num.com, Inc. All Rights Reserved
 * $Id$ 
 * 
 **************************************************************************/
 
 
 
/**
 * @file ice_transport_channel.h
 * @author str2num
 * @version $Revision$ 
 * @brief 
 *  
 **/



#ifndef  ICE_ICE_TRANSPORT_CHANNEL_H_
#define  ICE_ICE_TRANSPORT_CHANNEL_H_

#include <vector>
#include <string>

#include <rtc_base/third_party/sigslot/sigslot.h>
#include <rtc_base/thread.h>
#include <rtc_base/task_utils/pending_task_safety_flag.h>

#if defined(ICE_POSIX)
#include "ice/base/linux/event_loop.h"
#endif

#include "ice/ice_def.h"
#include "ice/port_allocator.h"
#include "ice/ice_credentials.h"
#include "ice/candidate.h"
#include "ice/stun.h"
#include "ice/ice_controller.h"

namespace ice {

class UDPPort;

enum class IceTransportState {
    kNew,
    kChecking,
    kConnected,
    kCompleted,
    kFailed,
    kDisconnected,
    kClosed,
};

class IceTransportChannel : public sigslot::has_slots<> {
public:
#if defined(ICE_POSIX)
    IceTransportChannel(EventLoop* el, PortAllocator* allocator,
            const std::string& transport_name,
            int component);
#elif defined(ICE_WIN)
    IceTransportChannel(rtc::Thread* network_thread, PortAllocator* allocator,
        const std::string& transport_name,
        int component);
#endif
    virtual ~IceTransportChannel();
    
    const std::string& transport_name() { return transport_name_; }
    int component() { return component_; }
    bool writable() { return writable_; }
    bool receiving() { return receiving_; }
    IceTransportState state() { return state_; }

    void set_ice_params(const IceParameters& ice_params);
    void set_remote_ice_params(const IceParameters& ice_params);
    void set_ice_role(IceRole role);
    void AddRemoteCandidate(const Candidate& candidate);
    void GatheringCandidate();
    int SendPacket(const char* data, size_t len);

    std::string ToString();

    sigslot::signal2<IceTransportChannel*, const std::vector<Candidate>&>
        SignalCandidateAllocateDone;
    sigslot::signal1<IceTransportChannel*> SignalReceivingState;
    sigslot::signal1<IceTransportChannel*> SignalWritableState;
    sigslot::signal1<IceTransportChannel*> SignalIceStateChanged;
    sigslot::signal4<IceTransportChannel*, const char*, size_t, int64_t> SignalReadPacket;

private:
    void OnUnknownAddress(UDPPort* port,
        const rtc::SocketAddress& addr,
        StunMessage* msg,
        const std::string& remote_ufrag);
    void AddConnection(IceConnection* conn);
    void SortConnectionsAndUpdateState(IceControllerEvent reason_to_sort);
    void MaybeStartPinging();
    void OnCheckAndPing();
    void OnConnectionStateChange(IceConnection* conn);
    void OnConnectionDestroyed(IceConnection* conn);
    void OnReadPacket(IceConnection* conn, const char* buf, size_t len, int64_t ts);

    void PingConnection(IceConnection* conn);
    void MaybeSwitchSelectedConnection(IceConnection* conn, IceControllerEvent reason);
    void SwitchSelectedConnection(IceConnection* conn);
    void UpdateConnectionStates();
    void UpdateState();
    void set_receiving(bool receiving);
    void set_writable(bool writable);
    IceTransportState ComputeIceTransportState();

    bool CreateConnections(const Candidate& remote_candidate);
    bool CreateConnection(UDPPort* port,
        const Candidate& remote_candidate);
    bool IsDuplicateRemoteCandidate(const Candidate& candidate);
    void RememberRemoteCandidate(const Candidate& remote_candidate);
    void FinishAddingRemoteCandidate(const Candidate& new_remote_candidate);

#if defined(ICE_POSIX)
    friend void IcePingCb(EventLoop* /*el*/, TimerWatcher* /*w*/, void* data);
#endif

private:
#if defined(ICE_POSIX)
    EventLoop* el_;
    TimerWatcher* ping_watcher_ = nullptr;
#elif defined(ICE_WIN)
    rtc::Thread* network_thread_;
#endif
    std::string transport_name_;
    int component_;
    PortAllocator* allocator_;
    IceParameters ice_params_;
    IceParameters remote_ice_params_;
    IceRole ice_role_ = IceRole::ICEROLE_UNKNOWN;
    std::vector<Candidate> local_candidates_;
    std::vector<Candidate> remote_candidates_;
    std::vector<UDPPort*> ports_;
    std::unique_ptr<IceController> ice_controller_;
    bool start_pinging_ = false;
    int cur_ping_interval_ = WEAK_PING_INTERVAL;
    int64_t last_ping_sent_ms_ = 0;
    IceConnection* selected_connection_ = nullptr;
    bool receiving_ = false;
    bool writable_ = false;
    IceTransportState state_ = IceTransportState::kNew;
    bool had_connection_ = false;
    bool has_been_connection_ = false;
    webrtc::ScopedTaskSafety task_safety_;
};

} // namespace ice

#endif  //__ICE_TRANSPORT_CHANNEL_H_


