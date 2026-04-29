/***************************************************************************
 * 
 * Copyright (c) 2022 str2num.com, Inc. All Rights Reserved
 * $Id$ 
 * 
 **************************************************************************/
 
 
 
/**
 * @file physical_socket_ex.h
 * @author str2num
 * @version $Revision$ 
 * @brief 
 *  
 **/



#ifndef  ICE_BASE_LINUX_PHYSICAL_SOCKET_EX_H_
#define  ICE_BASE_LINUX_PHYSICAL_SOCKET_EX_H_

#include <rtc_base/physical_socket_server.h>

namespace ice {

class PhysicalSocketEx : public rtc::PhysicalSocket {
public:
    PhysicalSocketEx() : PhysicalSocket(nullptr) {}

    SOCKET socket() { return s_; }

protected:
    void SetEnabledEvents(uint8_t /*events*/) override {}
    void EnableEvents(uint8_t /*events*/) override {}
    void DisableEvents(uint8_t /*events*/) override {}
};

} // namespace ice

#endif  // ICE_BASE_LINUX_PHYSICAL_SOCKET_EX_H_


