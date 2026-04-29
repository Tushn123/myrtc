/***************************************************************************
 * 
 * Copyright (c) 2022 vxrtc.com, Inc. All Rights Reserved
 * $Id$ 
 * 
 **************************************************************************/
 
 
 
/**
 * @file candidate.h
 * @version $Revision$ 
 * @brief 
 *  
 **/



#ifndef  ICE_ICE_CANDIDATE_H_
#define  ICE_ICE_CANDIDATE_H_

#include <string>

#include <rtc_base/socket_address.h>

#include "ice/ice_def.h"

namespace ice {

class Candidate {
public:
    static std::string ComputeFoundation(const std::string& type,
        const std::string& protocol,
        const std::string& relay_protocol,
        const rtc::SocketAddress& base);

    uint32_t GetPriority(uint32_t type_preference,
            int network_adapter_preference,
            int relay_preference);

    bool IsEquivalent(const Candidate& c) const;
    
    std::string ToString() const;

public:
    int component;
    std::string protocol;
    rtc::SocketAddress address;
    int port = 0;
    uint32_t priority;
    std::string username;
    std::string password;
    std::string type;
    std::string foundation;
};

} // namespace ice 

#endif  // ICE_ICE_CANDIDATE_H_


