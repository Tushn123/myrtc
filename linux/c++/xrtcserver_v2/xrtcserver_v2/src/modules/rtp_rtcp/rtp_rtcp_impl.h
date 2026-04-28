/***************************************************************************
 * 
 * Copyright (c) str2num.com, Inc. All Rights Reserved
 * $Id$ 
 * 
 **************************************************************************/
 
 
 
/**
 * @file rtp_rtcp_impl.h
 * @author str2num
 * @version $Revision$ 
 * @brief 
 *  
 **/



#ifndef  __XRTCSERVER_MODULES_RTP_RTCP_RTP_RTCP_IMPL_H_
#define  __XRTCSERVER_MODULES_RTP_RTCP_RTP_RTCP_IMPL_H_

#include "modules/rtp_rtcp/rtp_rtcp_config.h"
#include "modules/rtp_rtcp/rtcp_sender.h"
#include "modules/rtp_rtcp/rtcp_receiver.h"
#include "modules/rtp_rtcp/rtp_sender.h"

namespace xrtc {

class RtpRtcpImpl {
public:
    RtpRtcpImpl(const RtpRtcpConfig& config);
    ~RtpRtcpImpl();
    
    void SetRTCPStatus(webrtc::RtcpMode method);
    void TimeToSendRTCP();
    void IncomingRtcpPacket(const uint8_t* data, size_t len);
    void SetRemoteSsrc(uint32_t ssrc);
    void SendNack(const std::vector<uint16_t>& nack_list);
    void SendRTCP(webrtc::RTCPPacketType packet_type);
    void SetSendingStatus(bool sending);
    void UpdateRtpStat(int64_t now_ms, const webrtc::RtpPacketToSend& packet);
    void SetSrInfo(uint32_t rtp_timestamp, webrtc::NtpTime ntp);

private:
    RTCPSender::FeedbackState GetFeedbackState();

private:
    EventLoop* el_;
    RTCPSender rtcp_sender_;
    RTCPReceiver rtcp_receiver_;
    RtpSender rtp_sender_;

    TimerWatcher* rtcp_report_timer_ = nullptr;
    TimerWatcher* request_pli_timer_ = nullptr;
};

} // namespace xrtc


#endif  // __XRTCSERVER_MODULES_RTP_RTCP_RTP_RTCP_IMPL_H_


