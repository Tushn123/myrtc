/***************************************************************************
 * 
 * Copyright (c) str2num.com, Inc. All Rights Reserved
 * $Id$ 
 * 
 **************************************************************************/
 
 
 
/**
 * @file conf.h
 * @author str2num
 * @version $Revision$ 
 * @brief 
 *  
 **/



#ifndef  __XRTCSERVER_BASE_CONF_H_
#define  __XRTCSERVER_BASE_CONF_H_

#include <string>

namespace xrtc {

struct GeneralConf {
    std::string log_dir;
    std::string log_name;
    std::string log_level;
    bool log_to_stderr;
    int ice_min_port = 0;
    int ice_max_port = 0;
    // Public IPv4 to put in ICE host candidates when no NIC has a public address
    // (e.g. Aliyun ECS with only 172.x on eth0). UDP binds 0.0.0.0; SDP uses this IP.
    std::string ice_advertise_ipv4;
    int rtcp_report_timer_interval = 100;
};

int LoadGeneralConf(const char* filename, GeneralConf* conf);

} // namespace xrtc


#endif  //__XRTCSERVER_BASE_CONF_H_


