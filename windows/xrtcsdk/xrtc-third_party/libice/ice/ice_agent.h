/***************************************************************************
 * 
 * Copyright (c) 2022 ice.com, Inc. All Rights Reserved
 * $Id$ 
 * 
 **************************************************************************/
 
 
 
/**
 * @file ice_agent.h
 * @version $Revision$ 
 * @brief 
 *  
 **/



#ifndef ICE_ICE_AGENT_H_
#define ICE_ICE_AGENT_H_

#include <vector>
#include <string>

#include <rtc_base/thread.h>

#if defined(ICE_POSIX)
#include "ice/base/linux/event_loop.h"
#endif

#include "ice/ice_transport_channel.h"

#if defined(USE_DTLS)
#include "ice/dtls/dtls_transport_channel.h"
#include "ice/dtls/dtls_srtp_transport.h"
#include "ice/dtls/certificate.h"
#endif

namespace ice {

struct Fingerprint {
    std::string alg;
    std::string fingerprint;
};

enum class IceDtlsTransportState {
    kNew,
    kConnecting,
    kConnected,
    kDisconnected,
    kFailed,
    kClosed,
};

class IceAgent : public sigslot::has_slots<> {
public:
#if defined(ICE_POSIX)
    IceAgent(EventLoop* el, PortAllocator* allocator);
#elif defined(ICE_WIN)
    IceAgent(rtc::Thread* network_thread, PortAllocator* allocator);
#endif

    void Destroy();
    
    // ice基础部分
    bool CreateIceTransport(const std::string& transport_name,
            int component);
    void DestroyIceTransport(const std::string& transport_name,
            int component);
    void DestroyAllIceTransport();

    void SetIceParams(const IceParameters& ice_params);
    void SetIceParams(const std::string& transport_name,
            int component,
            const IceParameters& ice_params);
    void SetRemoteIceParams(const IceParameters& ice_params);
    void SetRemoteIceParams(const std::string& transport_name,
            int component,
            const IceParameters& ice_params);
    void SetIceRole(IceRole role);
    void SetIceRole(const std::string& transport_name,
        int component,
        IceRole role);
    IceTransportState ice_state() const { return ice_state_; }

    void AddRemoteCandidate(const std::string& host, int port, const IceParameters& ice_params);
    void AddRemoteCandidate(const Candidate& candidate);
    void AddRemoteCandidate(const std::string& transport_name, int component,
        const std::string& host, int port, const IceParameters& ice_params);
    void AddRemoteCandidate(const std::string& transport_name, int component,
        const Candidate& candidate);
    void GatheringCandidate();
    int SendPacket(const std::string& transport_name, int component, const char* data, size_t len);
    
    sigslot::signal4<IceAgent*, const std::string&, int,
        const std::vector<Candidate>&> SignalCandidateAllocateDone;
    sigslot::signal2<IceAgent*, IceTransportState> SignalIceState;
    sigslot::signal6<IceAgent*, const std::string&, int, const char*, size_t, int64_t> SignalReadPacket;

#if defined(USE_DTLS)
    // DTLS/SRTP扩展部分，不是ICE的标准，但是方便WebRTC peerconnection的建立以及加解密的传输
    static Fingerprint CreateFingerprintFromCertificate(Certificate* cert);
    bool CreateDtlsTransport(const std::string& transport_name,
            int component);
    void DestroyDtlsTransport(const std::string& transport_name,
            int component);
    void DestroyAllDtlsTransport();
    void SetDtlsRole(DtlsRole role);
    void SetLocalCertificate(Certificate* cert);
    bool SetRemoteFingerprint(const std::string& alg, const std::string& fingerprint);
    void UseDtlsSrtp(const std::string& transport_name);
    int SendSrtp(const std::string& transport_name, const char* data, size_t len);
    int SendSrtcp(const std::string& transport_name, const char* data, size_t len);

    sigslot::signal2<IceAgent*, IceDtlsTransportState> SignalIceDtlsState;
    sigslot::signal5<IceAgent*, const std::string&, const char*, size_t, int64_t> SignalUnSrtpPacket;
    sigslot::signal5<IceAgent*, const std::string&, const char*, size_t, int64_t> SignalUnSrtcpPacket;
#endif

private:
    ~IceAgent();

    IceTransportChannel* GetIceTransport(const std::string& transport_name,
            int component);
    std::vector<IceTransportChannel*>::iterator GetIceTransportInternal(
            const std::string& transport_name,
            int component); 
    void OnCandidateAllocateDone(IceTransportChannel* channel,
            const std::vector<Candidate>&);
    void OnIceReceivingState(IceTransportChannel* channel);
    void OnIceWritableState(IceTransportChannel* channel);
    void OnIceStateChanged(IceTransportChannel* channel);
    void OnReadPacket(IceTransportChannel* channel, const char* data, size_t len, int64_t ts);
    void UpdateState();

#if defined(USE_DTLS)
    DtlsTransportChannel* GetDtlsTransport(
        const std::string& transport_name,
        int component);
    std::vector<DtlsTransportChannel*>::iterator GetDtlsTransportInternal(
        const std::string& transport_name,
        int component);
    DtlsSrtpTransport* GetDtlsSrtpTransport(const std::string& transport_name);
    std::vector<DtlsSrtpTransport*>::iterator GetDtlsSrtpTransportInternal(
        const std::string& transport_name);
    void OnDtlsReceivingState(DtlsTransportChannel*);
    void OnDtlsWritableState(DtlsTransportChannel*);
    void OnDtlsState(DtlsTransportChannel*, DtlsTransportState);
    void OnUnSrtpPacket(DtlsSrtpTransport* dtls_srtp, rtc::CopyOnWriteBuffer* buf, int64_t ts);
    void OnUnSrtcpPacket(DtlsSrtpTransport* dtls_srtp, rtc::CopyOnWriteBuffer* buf, int64_t ts);
    void UpdateDtlsState();
#endif

#if defined(ICE_POSIX)
    friend void DestoryCb(EventLoop* el, TimerWatcher* w, void* data);
#endif

private:
#if defined(ICE_POSIX)
    EventLoop* el_;
    TimerWatcher* destroy_watcher_ = nullptr;
#elif defined(ICE_WIN)
    rtc::Thread* network_thread_ = nullptr;
    std::unique_ptr<rtc::Thread> inner_network_thread_;
#endif
    std::vector<IceTransportChannel*> ice_transports_;
    PortAllocator* allocator_;
    IceTransportState ice_state_ = IceTransportState::kNew;
    IceDtlsTransportState ice_dtls_state_ = IceDtlsTransportState::kNew;

#if defined(USE_DTLS)
    std::vector<DtlsTransportChannel*> dtls_transports_;
    std::vector<DtlsSrtpTransport*> dtls_srtp_transports_;
    bool dtls_ = false;
    bool dtls_srtp_ = false;
#endif

};

} // namespace ice

#endif  //ICE_ICE_AGENT_H_


