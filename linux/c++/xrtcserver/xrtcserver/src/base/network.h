/***************************************************************************
 * 
 * Copyright (c) 2022 str2num.com, Inc. All Rights Reserved
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



#ifndef  __BASE_NETWORK_H_
#define  __BASE_NETWORK_H_

#include <string>
#include <vector>
#include <rtc_base/ip_address.h>

namespace xrtc {

class Network {
public:
    // When bind_inaddr_any is true, UDP binds to 0.0.0.0 but ICE candidates use ip
    // (needed when the host has no public IPv4 on an interface, e.g. cloud NAT).
    Network(const std::string& name, const rtc::IPAddress& ip,
            bool bind_inaddr_any = false) :
        _name(name), _ip(ip), _bind_inaddr_any(bind_inaddr_any) {}
    ~Network() = default;

    const std::string& name() { return _name; } 
    const rtc::IPAddress& ip() { return _ip; }
    bool bind_inaddr_any() const { return _bind_inaddr_any; }
    
    std::string to_string() {
        return _name + ":" + _ip.ToString();
    }

private:
    std::string _name;
    rtc::IPAddress _ip;
    bool _bind_inaddr_any = false;
};

class NetworkManager {
public:
    NetworkManager();
    ~NetworkManager();
   
    const std::vector<Network*>& get_networks() { return _network_list; }
    // advertise_ipv4: when no interface yields a candidate, add this address for
    // ICE host candidate (bind all interfaces, advertise this IP in SDP).
    int create_networks(const std::string& advertise_ipv4 = "");

private:
    std::vector<Network*> _network_list;
};

} // namespace xrtc

#endif  //__BASE_NETWORK_H_


