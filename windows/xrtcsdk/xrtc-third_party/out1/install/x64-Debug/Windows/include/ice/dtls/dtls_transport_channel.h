/***************************************************************************
 * 
 * Copyright (c) 2022 str2num.com, Inc. All Rights Reserved
 * $Id$ 
 * 
 **************************************************************************/
 
 
 
/**
 * @file dtls_transport_channel.h
 * @author str2num
 * @version $Revision$ 
 * @brief 
 *  
 **/



#ifndef  ICE_DTLS_DTLS_TRANSPORT_CHANNEL_H_
#define  ICE_DTLS_DTLS_TRANSPORT_CHANNEL_H_

#include <memory>

#include <rtc_base/ssl_stream_adapter.h>
#include <rtc_base/buffer_queue.h>
#include <rtc_base/rtc_certificate.h>

#include "ice/ice_transport_channel.h"

namespace ice {

enum class DtlsTransportState {
    kNew,
    kConnecting,
    kConnected,
    kClosed,
    kFailed,
    kNumValues
};

class StreamInterfaceChannel : public rtc::StreamInterface {
public:
    StreamInterfaceChannel(IceTransportChannel* ice_channel);
    
    bool OnReceivedPacket(const char* data, size_t size);

    rtc::StreamState GetState() const override;
    rtc::StreamResult Read(void* buffer,
            size_t buffer_len,
            size_t* read,
            int* error) override;
    rtc::StreamResult Write(const void* data,
            size_t data_len,
            size_t* written,
            int* error) override;
    void Close() override;

private:
    IceTransportChannel* ice_channel_;
    rtc::BufferQueue packets_;
    rtc::StreamState state_ = rtc::SS_OPEN;
};

class DtlsTransportChannel : public sigslot::has_slots<> {
public:
    DtlsTransportChannel(IceTransportChannel* ice_channel);
    ~DtlsTransportChannel();
    
    const std::string& transport_name() { return ice_channel_->transport_name(); }
    int component() { return ice_channel_->component(); }
    IceTransportChannel* ice_channel() { return ice_channel_; }
    bool IsDtlsActive() { return dtls_active_; }
    bool writable() { return writable_; }
    
    int SendPacket(const char* data, size_t len);

    rtc::SSLRole GetDtlsRole() const;
    bool SetDtlsRole(rtc::SSLRole role);

    bool SetLocalCertificate(rtc::RTCCertificate* cert);
    bool SetRemoteFingerprint(const std::string& digest_alg,
            const uint8_t* digest, size_t digest_len);
    std::string ToString();
    DtlsTransportState dtls_state() { return dtls_state_; }
    bool GetSrtpCryptoSuite(int* selected_crypto_suite);
    bool ExportKeyingMaterial(const std::string& label,
            const uint8_t* context,
            size_t context_len,
            bool use_context,
            uint8_t* result,
            size_t result_len);

    sigslot::signal2<DtlsTransportChannel*, DtlsTransportState> SignalDtlsState;
    sigslot::signal1<DtlsTransportChannel*> SignalWritableState;
    sigslot::signal1<DtlsTransportChannel*> SignalReceivingState;
    sigslot::signal4<DtlsTransportChannel*, const char*, size_t, int64_t> SignalReadPacket;
    sigslot::signal1<DtlsTransportChannel*> SignalClosed;

private:
    void OnReadPacket(IceTransportChannel* /*channel*/,
            const char* buf, size_t len, int64_t ts);
    void OnDtlsEvent(rtc::StreamInterface* dtls, int sig, int error);
    void OnDtlsHandshakeError(rtc::SSLHandshakeError error);
    void OnReceivingState(IceTransportChannel* channel);
    void OnWritableState(IceTransportChannel* channel);
    bool SetupDtls();
    void MaybeStartDtls();
    void set_dtls_state(DtlsTransportState state);
    void set_writable_state(bool writable);
    void set_receiving(bool receiving);
    bool HandleDtlsPacket(const char* data, size_t size);

private:
    IceTransportChannel* ice_channel_;
    DtlsTransportState dtls_state_ = DtlsTransportState::kNew;
    bool receiving_ = false;
    bool writable_ = false;
    std::unique_ptr<rtc::SSLStreamAdapter> dtls_;
    StreamInterfaceChannel* downward_ = nullptr;
    rtc::Buffer cached_client_hello_;
    rtc::RTCCertificate* local_certificate_ = nullptr;
    rtc::Buffer remote_fingerprint_value_;
    std::string remote_fingerprint_alg_;
    bool dtls_active_ = false;
    std::vector<int> srtp_ciphers_;
    rtc::SSLRole dtls_role_ = rtc::SSL_CLIENT;
};

} // namespace ice

#endif  // ICE_DTLS_DTLS_TRANSPORT_CHANNEL_H_


