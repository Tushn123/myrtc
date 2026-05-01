/***************************************************************************
 * 
 * Copyright (c) str2num.com, Inc. All Rights Reserved
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

#include "ice/port_allocator.h"

#include "base/conf.h"

extern xrtc::GeneralConf* g_conf;

namespace xrtc {

PortAllocator::PortAllocator() :
    network_manager_(new NetworkManager())
{
    std::string advertise;
    if (g_conf) {
        advertise = g_conf->ice_advertise_ipv4;
    }
    network_manager_->CreateNetworks(advertise);
}

PortAllocator::~PortAllocator() = default;

const std::vector<Network*>& PortAllocator::GetNetworks() {
    return network_manager_->GetNetworks();
}

void PortAllocator::SetPortRange(int min_port, int max_port) {
    if (min_port > 0) {
        min_port_ = min_port;
    }

    if (max_port > 0) {
        max_port_ = max_port;
    }
}

} // namespace xrtc


