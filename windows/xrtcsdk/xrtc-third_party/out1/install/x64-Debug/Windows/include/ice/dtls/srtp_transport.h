/***************************************************************************
 * 
 * Copyright (c) 2022 str2num.com, Inc. All Rights Reserved
 * $Id$ 
 * 
 **************************************************************************/
 
 
 
/**
 * @file srtp_transport.h
 * @author str2num
 * @version $Revision$ 
 * @brief 
 *  
 **/



#ifndef  ICE_DTLS_SRTP_TRANSPORT_H_
#define  ICE_DTLS_SRTP_TRANSPORT_H_

#include <memory>
#include <string>
#include <vector>

#include <rtc_base/third_party/sigslot/sigslot.h>

#include "ice/dtls/srtp_session.h"

namespace ice {

class DtlsTransportChannel;

class SrtpTransport : public sigslot::has_slots<> {
public:
    SrtpTransport() = default;
    virtual ~SrtpTransport() = default;
    
    bool SetRtpParams(int send_cs,
            const uint8_t* send_key,
            size_t send_key_len,
            const std::vector<int>& send_extension_ids,
            int recv_cs,
            const uint8_t* recv_key,
            size_t recv_key_len,
            const std::vector<int>& recv_extension_ids);
    bool SetRtcpParams(int send_cs,
            const uint8_t* send_key,
            int send_key_len,
            const std::vector<int>& send_extension_ids,
            int recv_cs,
            const uint8_t* recv_key,
            int recv_key_len,
            const std::vector<int>& recv_extension_ids);
    void ResetParams();
    bool IsSrtpActive();
    bool IsWritable(bool rtcp);
    bool UnprotectRtp(void* p, int in_len, int* out_len);
    bool UnprotectRtcp(void* p, int in_len, int* out_len);
    bool ProtectRtp(void* p, int in_len, int max_len, int* out_len);
    bool ProtectRtcp(void* p, int in_len, int max_len, int* out_len);
    void GetSendAuthTagLen(int* rtp_auth_tag_len, int* rtcp_auth_tag_len);

    sigslot::signal1<bool> SignalWritableState;

protected:
    void MaybeUpdateWritableState();

private:
    void CreateSrtpSession();

protected:
    DtlsTransportChannel* rtp_dtls_transport_ = nullptr;
    DtlsTransportChannel* rtcp_dtls_transport_ = nullptr;
    std::unique_ptr<SrtpSession> send_session_;
    std::unique_ptr<SrtpSession> recv_session_;
    std::unique_ptr<SrtpSession> send_rtcp_session_;
    std::unique_ptr<SrtpSession> recv_rtcp_session_;
    bool writable_ = false;
};

} // namespace ice

#endif  // ICE_DTLS_SRTP_TRANSPORT_H_


