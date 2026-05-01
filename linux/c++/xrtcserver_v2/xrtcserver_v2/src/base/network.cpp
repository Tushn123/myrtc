/***************************************************************************
 * 
 * Copyright (c) str2num.com, Inc. All Rights Reserved
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

#include "base/network.h"

#include <ifaddrs.h>
#include <arpa/inet.h>

#include <rtc_base/logging.h>

namespace xrtc {

namespace {

bool ParseIpv4String(const std::string& s, rtc::IPAddress* out) {
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
    for (auto network : network_list_) {
        delete network;
    }

    network_list_.clear();
}

int NetworkManager::CreateNetworks(const std::string& advertise_ipv4) {
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

        RTC_LOG(LS_INFO) << "gathered network interface: " << network->ToString();

        network_list_.push_back(network);
    }
    
    freeifaddrs(interface);

    if (network_list_.empty() && !advertise_ipv4.empty()) {
        rtc::IPAddress pub;
        if (!ParseIpv4String(advertise_ipv4, &pub)) {
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
        network_list_.push_back(network);
    }

    return 0;
}

} // namespace xrtc


