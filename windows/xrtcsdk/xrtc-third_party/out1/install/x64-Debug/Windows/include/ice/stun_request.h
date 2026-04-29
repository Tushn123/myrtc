/***************************************************************************
 * 
 * Copyright (c) 2022 str2num.com, Inc. All Rights Reserved
 * $Id$ 
 * 
 **************************************************************************/
 
 
 
/**
 * @file stun_request.h
 * @author str2num
 * @version $Revision$ 
 * @brief 
 *  
 **/



#ifndef  ICE_STUN_REQUEST_H_
#define  ICE_STUN_REQUEST_H_

#include <map>

#include <rtc_base/third_party/sigslot/sigslot.h>

#include "ice/stun.h"

namespace ice {

class StunRequest;

class StunRequestManager {
public:
    StunRequestManager() = default;
    ~StunRequestManager();

    void Send(StunRequest* request);
    void Remove(StunRequest* request);
    bool CheckResponse(StunMessage* msg);

    sigslot::signal3<StunRequest*, const char*, size_t> SignalSendPacket;

private:
    typedef std::map<std::string, StunRequest*> RequestMap;
    RequestMap requests_;
};

class StunRequest {
public:
    StunRequest(StunMessage* request);
    virtual ~StunRequest();
   
    int type() { return msg_->type(); }
    const std::string& id() { return msg_->transaction_id(); }
    void set_manager(StunRequestManager* manager) { manager_ = manager; }
    void Construct();
    void Send();
    int Elapsed();

protected:
    virtual void Prepare(StunMessage*) {}
    virtual void OnRequestResponse(StunMessage*) {}
    virtual void OnRequestErrorResponse(StunMessage*) {}
    
    friend class StunRequestManager;

private:
    StunMessage* msg_;
    StunRequestManager* manager_ = nullptr;
    int64_t ts_ = 0;
};

} // namespace ice

#endif  // ICE_STUN_REQUEST_H_


