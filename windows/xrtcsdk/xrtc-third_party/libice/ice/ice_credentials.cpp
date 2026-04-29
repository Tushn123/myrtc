/***************************************************************************
 * 
 * Copyright (c) 2022 str2num.com, Inc. All Rights Reserved
 * $Id$ 
 * 
 **************************************************************************/
 
 
 
/**
 * @file ice_credentials.cpp
 * @author str2num
 * @version $Revision$ 
 * @brief 
 *  
 **/

#include "ice/ice_credentials.h"

#include <rtc_base/helpers.h>

#include "ice/ice_def.h"

namespace ice {

IceParameters IceCredentials::CreateRandomIceCredentials() {
    return IceParameters(rtc::CreateRandomString(ICE_UFRAG_LENGTH),
            rtc::CreateRandomString(ICE_PWD_LENGTH));
}

} // namespace ice


