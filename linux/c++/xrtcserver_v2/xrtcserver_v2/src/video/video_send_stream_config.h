/***************************************************************************
 * 
 * Copyright (c) str2num.com, Inc. All Rights Reserved
 * $Id$ 
 * 
 **************************************************************************/
 
 
 
/**
 * @file video_send_stream_config.h
 * @author str2num
 * @version $Revision$ 
 * @brief 
 *  
 **/



#ifndef  __XRTCSERVER_VIDEO_VIDEO_SEND_STREAM_CONFIG_H_
#define  __XRTCSERVER_VIDEO_VIDEO_SEND_STREAM_CONFIG_H_

#include "base/event_loop.h"
#include "modules/rtp_rtcp/rtp_rtcp_config.h"

#include <system_wrappers/include/clock.h>

namespace xrtc {

class VideoSendStreamConfig {
public:
    EventLoop* el = nullptr;
    webrtc::Clock* clock = nullptr;
    RtpRtcpModuleObserver* rtp_rtcp_module_observer = nullptr;

    struct Rtp {
        uint32_t local_ssrc = 0;
        uint32_t local_rtx_ssrc = 0;
    } rtp;
};


} // namespace xrtc

#endif  //__XRTCSERVER_VIDEO_VIDEO_SEND_STREAM_CONFIG_H_


