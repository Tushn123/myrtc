/***************************************************************************
 * 
 * Copyright (c) str2num.com, Inc. All Rights Reserved
 * $Id$ 
 * 
 **************************************************************************/
 
 
 
/**
 * @file rtp_frame_object.cpp
 * @author str2num
 * @version $Revision$ 
 * @brief 
 *  
 **/

#include "modules/video_coding/rtp_frame_object.h"

namespace xrtc {

RtpFrameObject::RtpFrameObject(uint16_t first_seq_num,
        uint16_t last_seq_num,
        webrtc::VideoCodecType codec_type,
        const webrtc::RTPVideoHeader& video_header) :
    first_seq_num_(first_seq_num),
    last_seq_num_(last_seq_num),
    codec_type_(codec_type),
    video_header_(video_header) {}

RtpFrameObject::~RtpFrameObject() {}


} // namespace xrtc


