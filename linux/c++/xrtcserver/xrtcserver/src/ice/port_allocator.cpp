/***************************************************************************
 * 
 * Copyright (c) 2022 str2num.com, Inc. All Rights Reserved
 * $Id$ 
 * 
 **************************************************************************/
 
 
 
/**
 * @file port_allocator.cpp
 * @author str2num
 * @version $Revision$ 
 * @brief 
 *  
 **/

#include "base/conf.h"
#include "ice/port_allocator.h"

extern xrtc::GeneralConf* g_conf;

namespace xrtc {

PortAllocator::PortAllocator() :
    _network_manager(new NetworkManager())
{
    std::string advertise;
    if (g_conf) {
        advertise = g_conf->ice_advertise_ipv4;
    }
    _network_manager->create_networks(advertise);
}

PortAllocator::~PortAllocator() = default;

const std::vector<Network*>& PortAllocator::get_networks() {
    return _network_manager->get_networks();
}

void PortAllocator::set_port_range(int min_port, int max_port) {
    if (min_port > 0) {
        _min_port = min_port;
    }

    if (max_port > 0) {
        _max_port = max_port;
    }
}

} // namespace xrtc


