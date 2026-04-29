/***************************************************************************
 * 
 * Copyright (c) 2022 ice.com, Inc. All Rights Reserved
 * $Id$ 
 * 
 **************************************************************************/
 
 
 
/**
 * @file port_allocator.cpp
 * @version $Revision$ 
 * @brief 
 *  
 **/

#include "ice/port_allocator.h"

namespace ice {

PortAllocator::PortAllocator(const NetworkConfig& config) :
    network_manager_(new NetworkManager())
{
    network_manager_->CreateNetworks(config);
}

PortAllocator::~PortAllocator() = default;

const std::vector<Network*>& PortAllocator::GetNetworks() {
    return network_manager_->get_networks();
}

void PortAllocator::SetPortRange(int min_port, int max_port) {
    if (min_port > 0) {
        min_port_ = min_port;
    }

    if (max_port > 0) {
        max_port_ = max_port;
    }
}

} // namespace ice


