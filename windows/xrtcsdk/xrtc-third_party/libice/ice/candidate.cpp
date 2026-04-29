/***************************************************************************
 * 
 * Copyright (c) 2022 vxrtc.com, Inc. All Rights Reserved
 * $Id$ 
 * 
 **************************************************************************/
 
 
 
/**
 * @file candidate.cpp
 * @version $Revision$ 
 * @brief 
 *  
 **/

#include "ice/candidate.h"

#include <sstream>

#include <rtc_base/crc32.h>

namespace ice {

std::string Candidate::ComputeFoundation(const std::string& type,
    const std::string& protocol,
    const std::string& relay_protocol,
    const rtc::SocketAddress& base)
{
    std::stringstream ss;
    ss << type << base.HostAsURIString() << protocol << relay_protocol;
    return std::to_string(rtc::ComputeCrc32(ss.str()));
}

/*
priority = (2^24)*(type preference) +
           (2^8)*(local preference) +
           (2^0)*(256 - component ID)
*/
// 0	1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |  NIC Pref     |    Addr Pref  |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// NIC type：3G/wifi
// Addr pref: 定义在RFC3484
uint32_t Candidate::GetPriority(uint32_t type_preference,
        int network_adapter_preference,
        int relay_preference)
{
    int addr_ref = rtc::IPAddressPrecedence(address.ipaddr());
	int local_pref = ((network_adapter_preference << 8) | addr_ref) + relay_preference;
    return (type_preference << 24) | (local_pref << 8) | (256 - (int)component);
}

bool Candidate::IsEquivalent(const Candidate& c) const {
    return (component == c.component) && (protocol == c.protocol) &&
        (address == c.address) && (username == c.username) &&
        (password == c.password) && (type == c.type) &&
        (foundation == c.foundation);
}

std::string Candidate::ToString() const {
    std::stringstream ss;
    ss << "Cand[" << foundation << ":" << component << ":" << protocol
        << ":" << priority << ":" << address.ToString() << ":" << type
        << ":" << username << ":" << password << "]";
    return ss.str();
}


} // namespace ice



