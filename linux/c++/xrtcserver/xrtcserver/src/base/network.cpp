/***************************************************************************
 * 
 * Copyright (c) 2022 str2num.com, Inc. All Rights Reserved
 * $Id$ 
 * 
 **************************************************************************/
 
 
 
/**
 * @file network.cpp
 * @author str2num
 * @version $Revision$ 
 * @brief 
 *  
 **/

#include <ifaddrs.h>
#include <arpa/inet.h>

#include <rtc_base/logging.h>
#include "base/network.h"

namespace xrtc {

namespace {

bool parse_ipv4_string(const std::string& s, rtc::IPAddress* out) {
    if (!out || s.empty()) {
        return false;
    }
    struct in_addr a;
    if (inet_pton(AF_INET, s.c_str(), &a) != 1) {
        return false;
    }
    *out = rtc::IPAddress(a);
    return true;
}

} // namespace

NetworkManager::NetworkManager() = default;

NetworkManager::~NetworkManager() {
    for (auto network : _network_list) {
        delete network;
    }

    _network_list.clear();
}

int NetworkManager::create_networks(const std::string& advertise_ipv4) {
    struct ifaddrs* interface;
    int err = getifaddrs(&interface);
    if (err != 0) {
        RTC_LOG(LS_WARNING) << "getifaddrs error: " << strerror(errno) 
            << ", errno: " << errno;
        return -1;
    }
    
    for (auto cur = interface; cur != nullptr; cur = cur->ifa_next) {
        if (!cur->ifa_addr || cur->ifa_addr->sa_family != AF_INET) {
            continue;
        }

        struct sockaddr_in* addr = (struct sockaddr_in*)(cur->ifa_addr);
        rtc::IPAddress ip_address(addr->sin_addr);
        
        if (rtc::IPIsPrivateNetwork(ip_address) || rtc::IPIsLoopback(ip_address)) {
            continue;
        }
        
        Network* network = new Network(cur->ifa_name, ip_address);

        RTC_LOG(LS_INFO) << "gathered network interface: " << network->to_string();

        _network_list.push_back(network);
    }
    
    freeifaddrs(interface);

    if (_network_list.empty() && !advertise_ipv4.empty()) {
        rtc::IPAddress pub;
        if (!parse_ipv4_string(advertise_ipv4, &pub)) {
            RTC_LOG(LS_WARNING) << "ice advertise_ip is not a valid IPv4: "
                << advertise_ipv4;
            return 0;
        }
        if (rtc::IPIsPrivateNetwork(pub) || rtc::IPIsLoopback(pub)) {
            RTC_LOG(LS_WARNING) << "ice advertise_ip should be a public routable "
                << "IPv4 for internet clients; got: " << advertise_ipv4;
        }
        auto* network = new Network("advertise", pub, true);
        RTC_LOG(LS_INFO) << "no public interface found; ICE host candidate uses "
            << "advertise_ip=" << advertise_ipv4 << " (bind INADDR_ANY)";
        _network_list.push_back(network);
    }

    return 0;
}

} // namespace xrtc


