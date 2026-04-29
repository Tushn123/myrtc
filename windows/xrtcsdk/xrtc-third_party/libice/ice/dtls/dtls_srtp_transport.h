/***************************************************************************
 * 
 * Copyright (c) 2022 str2num.com, Inc. All Rights Reserved
 * $Id$ 
 * 
 **************************************************************************/
 
 
 
/**
 * @file dtls_srtp_transport.h
 * @author str2num
 * @version $Revision$ 
 * @brief 
 *  
 **/



#ifndef  ICE_DTLS_DTLS_SRTP_TRANSPORT_H_
#define  ICE_DTLS_DTLS_SRTP_TRANSPORT_H_

#include <string>

#include <rtc_base/buffer.h>
#include <rtc_base/copy_on_write_buffer.h>

#include "ice/dtls/srtp_transport.h"

namespace ice {

class DtlsTransportChannel;
enum class DtlsTransportState;

class DtlsSrtpTransport : public SrtpTransport {
public:
    DtlsSrtpTransport(const std::string& transport_name);
    
    void SetDtlsTransports(DtlsTransportChannel* rtp_dtls_transport,
            DtlsTransportChannel* rtcp_dtls_transport);
    bool IsDtlsWritable();
    const std::string& transport_name() { return transport_name_; }
    int SendSrtp(const char* data, size_t len);
    int SendSrtcp(const char* data, size_t len);

    sigslot::signal3<DtlsSrtpTransport*, rtc::CopyOnWriteBuffer*, int64_t>
        SignalUnSrtpPacketReceived;
    sigslot::signal3<DtlsSrtpTransport*, rtc::CopyOnWriteBuffer*, int64_t>
        SignalUnSrtcpPacketReceived;

private:
    bool ExtractParams(DtlsTransportChannel* dtls_transport,
            int* selected_crypto_suite,
            rtc::ZeroOnFreeBuffer<unsigned char>* send_key,
            rtc::ZeroOnFreeBuffer<unsigned char>* recv_key);
    void MaybeSetupDtlsSrtp();
    void SetupDtlsSrtp();
    void SetupRtcpDtlsSrtp();
    void OnDtlsState(DtlsTransportChannel* dtls, DtlsTransportState state);
    void OnReadPacket(DtlsTransportChannel* dtls, const char* data, size_t len, int64_t ts);
    void OnRtpPacketReceived(rtc::CopyOnWriteBuffer packet, int64_t ts);
    void OnRtcpPacketReceived(rtc::CopyOnWriteBuffer packet, int64_t ts);

private:
    std::string transport_name_;
    int unprotect_fail_count_ = 0;
    uint16_t last_send_seq_num_ = 0;
};

} // namespace ice

#endif  // ICE_DTLS_DTLS_SRTP_TRANSPORT_H_


