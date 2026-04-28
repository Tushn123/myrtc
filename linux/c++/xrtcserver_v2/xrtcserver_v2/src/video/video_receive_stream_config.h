/***************************************************************************
 * 
 * Copyright (c) str2num.com, Inc. All Rights Reserved
 * $Id$ 
 * 
 **************************************************************************/
 
 
 
/**
 * @file video_receive_stream_config.h
 * @author str2num
 * @version $Revision$ 
 * @brief 
 *  
 **/



#ifndef  XRTCSERVER_VIDEO_VIDEO_RECEIVE_STREAM_CONFIG_H_
#define  XRTCSERVER_VIDEO_VIDEO_RECEIVE_STREAM_CONFIG_H_

#include <map>

#include <system_wrappers/include/clock.h>

#include "base/event_loop.h"

namespace xrtc {

class RtpRtcpModuleObserver;

class VideoReceiveStreamConfig {
public:
    EventLoop* el = nullptr;
    webrtc::Clock* clock = nullptr;

    struct Rtp {
        uint32_t local_ssrc = 0;
        uint32_t remote_ssrc = 0;
        uint32_t rtx_ssrc = 0;
        std::map<int, int> rtx_associated_payload_types;
    } rtp;

    RtpRtcpModuleObserver* rtp_rtcp_module_observer = nullptr;
    int request_pli_interval_ms = 0;
};

} // namespace xrtc

#endif  //XRTCSERVER_VIDEO_VIDEO_RECEIVE_STREAM_CONFIG_H_


