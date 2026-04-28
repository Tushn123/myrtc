/***************************************************************************
 * 
 * Copyright (c) str2num.com, Inc. All Rights Reserved
 * $Id$ 
 * 
 **************************************************************************/
 
 
 
/**
 * @file rtx_receive_stream.h
 * @author str2num
 * @version $Revision$ 
 * @brief 
 *  
 **/



#ifndef  __XRTCSERVER_VIDEO_RTX_RECEIVE_STREAM_H_
#define  __XRTCSERVER_VIDEO_RTX_RECEIVE_STREAM_H_

#include <map>

#include <modules/rtp_rtcp/source/rtp_packet_received.h>

namespace xrtc {

class RtpVideoStreamReceiver;
class ReceiveStat;

class RtxReceiveStream {
public:
    RtxReceiveStream(RtpVideoStreamReceiver* media_sink,
            uint32_t media_ssrc,
            const std::map<int, int>& rtx_associated_payload_types,
            ReceiveStat* rtp_receive_stat);
    ~RtxReceiveStream();

    void OnRtpPacket(const webrtc::RtpPacketReceived& rtx_packet);

private:
    RtpVideoStreamReceiver* media_sink_;
    uint32_t media_ssrc_;
    std::map<int, int> rtx_associated_payload_types_;
    ReceiveStat* rtp_receive_stat_;
};

} // namespace xrtc

#endif  //__XRTCSERVER_VIDEO_RTX_RECEIVE_STREAM_H_


