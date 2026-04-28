/***************************************************************************
 * 
 * Copyright (c) str2num.com, Inc. All Rights Reserved
 * $Id$ 
 * 
 **************************************************************************/
 
 
 
/**
 * @file video_receive_stream.h
 * @author str2num
 * @version $Revision$ 
 * @brief 
 *  
 **/



#ifndef  XRTCSERVER_VIDEO_VIDEO_RECEIVE_STREAM_H_
#define  XRTCSERVER_VIDEO_VIDEO_RECEIVE_STREAM_H_

#include "video/video_receive_stream_config.h"
#include "video/rtp_video_stream_receiver.h"
#include "video/rtx_receive_stream.h"

namespace xrtc {

class VideoReceiveStream {
public:
    VideoReceiveStream(const VideoReceiveStreamConfig& config);
    ~VideoReceiveStream();
    
    void OnRtpPacket(const webrtc::RtpPacketReceived& packet);
    void DeliverRtcp(const uint8_t* data, size_t len);

private:
    VideoReceiveStreamConfig config_;
    std::unique_ptr<ReceiveStat> rtp_receive_stat_;
    RtpVideoStreamReceiver rtp_video_stream_receiver_;
    std::unique_ptr<RtxReceiveStream> rtx_receive_stream_;
};

} // namespace xrtc

#endif  //XRTCSERVER_VIDEO_VIDEO_RECEIVE_STREAM_H_


