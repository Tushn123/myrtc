/***************************************************************************
 * 
 * Copyright (c) 2022 vxrtc.com, Inc. All Rights Reserved
 * $Id$ 
 * 
 **************************************************************************/
 
 
 
/**
 * @file network.h
 * @version $Revision$ 
 * @brief 
 *  
 **/



#ifndef  ICE_BASE_NETWORK_H_
#define  ICE_BASE_NETWORK_H_

#include <vector>

#include <rtc_base/ip_address.h>

namespace ice {

struct NetworkConfig {
    bool disable_private_ip = false;
    bool disable_ipv6 = true;
};

class Network {
public:
    Network(const std::string& name, const rtc::IPAddress& ip) :
        name_(name), ip_(ip) {}
    ~Network() = default;

    const std::string& name() { return name_; } 
    const rtc::IPAddress& ip() { return ip_; }
    
    std::string ToString() {
        return name_ + ":" + ip_.ToString();
    }

private:
    std::string name_;
    rtc::IPAddress ip_;
};

class NetworkManager {
public:
    NetworkManager();
    ~NetworkManager();
   
    const std::vector<Network*>& get_networks() { return network_list_; }
    int CreateNetworks(const NetworkConfig& config);

private:
    std::vector<Network*> network_list_;
};

} // namespace ice 

#endif  // ICE_BASE_NETWORK_H_


