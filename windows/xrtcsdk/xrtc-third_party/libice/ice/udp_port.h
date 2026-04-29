/***************************************************************************
 * 
 * Copyright (c) 2022 str2num.com, Inc. All Rights Reserved
 * $Id$ 
 * 
 **************************************************************************/
 
 
 
/**
 * @file udp_port.h
 * @author str2num
 * @version $Revision$ 
 * @brief 
 *  
 **/



#ifndef  ICE_UDP_PORT_H_
#define  ICE_UDP_PORT_H_

#include <string>
#include <vector>
#include <map>

#include <rtc_base/socket_address.h>
#include <rtc_base/thread.h>
#include <rtc_base/async_udp_socket.h>

#if defined(ICE_POSIX)
#include "ice/base/linux/event_loop.h"
#endif

#include "base/network.h"
#include "base/linux/async_udp_socket.h"
#include "ice/ice_def.h"
#include "ice/ice_credentials.h"
#include "ice/candidate.h"
#include "ice/stun.h"

namespace ice {

class IceConnection;

typedef std::map<rtc::SocketAddress, IceConnection*> AddressMap;

class UDPPort : public sigslot::has_slots<> {
public:
#if defined(ICE_POSIX)
    UDPPort(EventLoop* el,
            const std::string& transport_name,
            int component,
            IceParameters ice_params);
#elif defined(ICE_WIN)
    UDPPort(rtc::Thread* network_thread,
        const std::string& transport_name,
        int component,
        IceParameters ice_params);
#endif
    ~UDPPort();
        
    std::string ice_ufrag() { return ice_params_.ice_ufrag; }
    std::string ice_pwd() { return ice_params_.ice_pwd; }

    const std::string& transport_name() { return transport_name_; }
    int component() { return component_; }
    const rtc::SocketAddress& local_addr() { return local_addr_; }
    const std::vector<Candidate>& candidates() { return candidates_; }

    IceRole ice_role() { return ice_role_; }
    void set_ice_role(IceRole role) { ice_role_ = role; }

    int CreateIceCandidate(Network* network, int min_port, int max_port, Candidate& c);
    bool GetStunMessage(const char* data, size_t len,
            const rtc::SocketAddress& addr,
            std::unique_ptr<StunMessage>* out_msg,
            std::string* out_username);
    void SendBindingErrorResponse(StunMessage* stun_msg,
            const rtc::SocketAddress& addr,
            int err_code,
            const std::string& reason);
    IceConnection* CreateConnection(const Candidate& candidate);
    IceConnection* GetConnection(const rtc::SocketAddress& addr);
    void CreateStunUserName(const std::string& remote_username, 
            std::string* stun_attr_username);

    int SendTo(const char* buf, size_t len, const rtc::SocketAddress& addr);

    std::string ToString();
    
    sigslot::signal4<UDPPort*, const rtc::SocketAddress&, StunMessage*, const std::string&>
        SignalUnknownAddress;

private:
    int BindSocket(rtc::Socket* socket,
            const rtc::SocketAddress& local_address,
            uint16_t min_port,
            uint16_t max_port);
    void OnReadPacket(rtc::AsyncPacketSocket* socket, const char* buf, size_t size,
            const rtc::SocketAddress& addr, const int64_t& ts);
    bool ParseStunUserName(StunMessage* stun_msg, std::string* local_ufrag,
            std::string* remote_frag);

private:
#if defined(ICE_POSIX)
    EventLoop* el_;
    std::unique_ptr<AsyncUdpSocket> async_socket_;
#elif defined(ICE_WIN)
    rtc::Thread* network_thread_;
    std::unique_ptr<rtc::AsyncUDPSocket> async_socket_;
#endif
    std::string transport_name_;
    int component_;
    IceParameters ice_params_;
    IceRole ice_role_ = IceRole::ICEROLE_UNKNOWN;
    rtc::SocketAddress local_addr_;
    std::vector<Candidate> candidates_;
    AddressMap connections_;
};

} // namespace ice 

#endif  // ICE_UDP_PORT_H_


