/***************************************************************************
 * 
 * Copyright (c) 2022 vxrtc.com, Inc. All Rights Reserved
 * $Id$ 
 * 
 **************************************************************************/
 
 
 
/**
 * @file network.cpp
 * @version $Revision$ 
 * @brief 
 *  
 **/

#include "ice/base/network.h"

#if defined(ICE_POSIX)
#include <ifaddrs.h>
#elif defined(ICE_WIN)
#include <iphlpapi.h>

#include "rtc_base/win32.h"
#endif

#include <rtc_base/logging.h>
#include <rtc_base/string_utils.h>

namespace ice {

NetworkManager::NetworkManager() = default;

NetworkManager::~NetworkManager() {
    for (auto network : network_list_) {
        delete network;
    }

    network_list_.clear();
}

#if defined(ICE_POSIX)

int NetworkManager::CreateNetworks(const NetworkConfig& config) {
    struct ifaddrs* interface;
    int err = getifaddrs(&interface);
    if (err != 0) {
        RTC_LOG(LS_WARNING) << "getifaddrs error: " << strerror(errno) 
            << ", errno: " << errno;
        return -1;
    }
    
    for (auto cur = interface; cur != nullptr; cur = cur->ifa_next) {
        if (cur->ifa_addr->sa_family != AF_INET) {
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

    return 0;
}

#elif defined(ICE_WIN)
 
int NetworkManager::CreateNetworks(const NetworkConfig& config) {
    // MSDN recommends a 15KB buffer for the first try at GetAdaptersAddresses.
    size_t buffer_size = 16384;
    std::unique_ptr<char[]> adapter_info(new char[buffer_size]);
    PIP_ADAPTER_ADDRESSES adapter_addrs =
        reinterpret_cast<PIP_ADAPTER_ADDRESSES>(adapter_info.get());
    int adapter_flags = (GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_SKIP_ANYCAST |
        GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_INCLUDE_PREFIX);
    int ret = 0;
    do {
        adapter_info.reset(new char[buffer_size]);
        adapter_addrs = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(adapter_info.get());
        ret = GetAdaptersAddresses(AF_UNSPEC, adapter_flags, 0, adapter_addrs,
            reinterpret_cast<PULONG>(&buffer_size));
    } while (ret == ERROR_BUFFER_OVERFLOW);

    if (ret != ERROR_SUCCESS) {
        return -1;
    }

    int count = 0;
    while (adapter_addrs) {
        if (adapter_addrs->OperStatus == IfOperStatusUp) {
            PIP_ADAPTER_UNICAST_ADDRESS address = adapter_addrs->FirstUnicastAddress;
            PIP_ADAPTER_PREFIX prefixlist = adapter_addrs->FirstPrefix;
            std::string name;
            std::string description;
            description = rtc::ToUtf8(adapter_addrs->Description,
                wcslen(adapter_addrs->Description));
            for (; address; address = address->Next) {
                // 过滤掉IPv6
                if (config.disable_ipv6 && address->Address.lpSockaddr->sa_family != AF_INET) {
                    continue;
                }

                name = rtc::ToString(count);

                sockaddr_in* v4_addr =
                    reinterpret_cast<sockaddr_in*>(address->Address.lpSockaddr);
                rtc::IPAddress ip_address(v4_addr->sin_addr);

                if (config.disable_private_ip && rtc::IPIsPrivateNetwork(ip_address)) {
                    continue;
                }

                if (rtc::IPIsLoopback(ip_address)) {
                    continue;
                }

                Network* network = new Network(name, ip_address);

                RTC_LOG(LS_INFO) << "gathered network interface: " << network->ToString();

                network_list_.push_back(network);
            }

            ++count;
        }

        adapter_addrs = adapter_addrs->Next;
    }

    return 0;
}

#endif // ICE_WIN

} // namespace ice 


