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
    for (auto network : network_list_) {
        delete network;
    }

    network_list_.clear();
}

<<<<<<< HEAD
int NetworkManager::CreateNetworks(const std::string& advertise_ipv4) {
=======
int NetworkManager::CreateNetworks() {
>>>>>>> 0fa258316b07f8586ed8420d9aa473420bb7c048
    struct ifaddrs* interface;
    if (err != 0) {
    }
    
    for (auto cur = interface; cur != nullptr; cur = cur->ifa_next) {
<<<<<<< HEAD
        if (!cur->ifa_addr || cur->ifa_addr->sa_family != AF_INET) {
=======
        if (cur->ifa_addr->sa_family != AF_INET) {
>>>>>>> 0fa258316b07f8586ed8420d9aa473420bb7c048
            continue;
        }

        struct sockaddr_in* addr = (struct sockaddr_in*)(cur->ifa_addr);
        rtc::IPAddress ip_address(addr->sin_addr);
        if (rtc::IPIsPrivateNetwork(ip_address) || rtc::IPIsLoopback(ip_address)) {
        Network* network = new Network(cur->ifa_name, ip_address);

        RTC_LOG(LS_INFO) << "gathered network interface: " << network->ToString();

        network_list_.push_back(network);
    }
    
    freeifaddrs(interface);

<<<<<<< HEAD
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
        network_list_.push_back(network);
    }

=======
>>>>>>> 0fa258316b07f8586ed8420d9aa473420bb7c048
    return 0;
}

} // namespace xrtc


