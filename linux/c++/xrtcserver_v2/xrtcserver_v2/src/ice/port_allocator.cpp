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

<<<<<<< HEAD
#include "base/conf.h"

extern xrtc::GeneralConf* g_conf;

=======
>>>>>>> 0fa258316b07f8586ed8420d9aa473420bb7c048
namespace xrtc {

PortAllocator::PortAllocator() :
    network_manager_(new NetworkManager())
{
<<<<<<< HEAD
    std::string advertise;
    if (g_conf) {
        advertise = g_conf->ice_advertise_ipv4;
    }
    network_manager_->CreateNetworks(advertise);
=======
    network_manager_->CreateNetworks();
>>>>>>> 0fa258316b07f8586ed8420d9aa473420bb7c048
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


