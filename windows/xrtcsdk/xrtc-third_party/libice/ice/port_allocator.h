/***************************************************************************
 * 
 * Copyright (c) 2022 ice.com, Inc. All Rights Reserved
 * $Id$ 
 * 
 **************************************************************************/
 
 
 
/**
 * @file port_allocator.h
 * @version $Revision$ 
 * @brief 
 *  
 **/



#ifndef ICE_PORT_ALLOCATOR_H_
#define ICE_PORT_ALLOCATOR_H_

#include <memory>

#include "ice/base/network.h"

namespace ice {

class PortAllocator {
public:
    PortAllocator(const NetworkConfig& config);
    ~PortAllocator();
    
    const std::vector<Network*>& GetNetworks();
    
    void SetPortRange(int min_port, int max_port);

    int min_port() { return min_port_; }
    int max_port() { return max_port_; }

private:
    std::unique_ptr<NetworkManager> network_manager_;
    int min_port_ = 0;
    int max_port_ = 0;
};

} // namespace ice

#endif  //ICE_PORT_ALLOCATOR_H_


