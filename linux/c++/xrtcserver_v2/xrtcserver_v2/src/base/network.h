/***************************************************************************
 * 
 * Copyright (c) str2num.com, Inc. All Rights Reserved
 * $Id$ 
 * 
 **************************************************************************/
 
 
 
/**
 * @file network.h
 * @author str2num
 * @version $Revision$ 
 * @brief 
 *  
 **/



#ifndef  __XRTCSERVER_BASE_NETWORK_H_
#define  __XRTCSERVER_BASE_NETWORK_H_

#include <vector>

#include <rtc_base/ip_address.h>

namespace xrtc {

class Network {
public:
    // When bind_inaddr_any is true, UDP binds to 0.0.0.0 but ICE candidates use ip
    // (needed when the host has no public IPv4 on an interface, e.g. cloud NAT).
    Network(const std::string& name, const rtc::IPAddress& ip,
            bool bind_inaddr_any = false) :
        name_(name), ip_(ip), bind_inaddr_any_(bind_inaddr_any) {}
    ~Network() = default;

    const std::string& name() { return name_; } 
    const rtc::IPAddress& ip() { return ip_; }
    bool bind_inaddr_any() const { return bind_inaddr_any_; }
    
    std::string ToString() {
        return name_ + ":" + ip_.ToString();
    }

private:
    std::string name_;
    rtc::IPAddress ip_;
    bool bind_inaddr_any_ = false;
};

class NetworkManager {
public:
    NetworkManager();
    ~NetworkManager();
   
    const std::vector<Network*>& GetNetworks() { return network_list_; }
    // advertise_ipv4: when no interface yields a candidate, add this address for
    // ICE host candidate (bind all interfaces, advertise this IP in SDP).
    int CreateNetworks(const std::string& advertise_ipv4 = "");

private:
    std::vector<Network*> network_list_;
};

} // namespace xrtc

#endif  //__XRTCSERVER_BASE_NETWORK_H_


