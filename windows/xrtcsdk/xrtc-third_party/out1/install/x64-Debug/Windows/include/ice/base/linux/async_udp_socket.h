/***************************************************************************
 * 
 * Copyright (c) 2022 str2num.com, Inc. All Rights Reserved
 * $Id$ 
 * 
 **************************************************************************/
 
 
 
/**
 * @file async_udp_socket.h
 * @author str2num
 * @version $Revision$ 
 * @brief 
 *  
 **/



#ifndef ICE_BASE_LINUX_ASYNC_UDP_SOCKET_H_
#define ICE_BASE_LINUX_ASYNC_UDP_SOCKET_H_

#include <list>

#include <rtc_base/third_party/sigslot/sigslot.h>
#include <rtc_base/socket_address.h>
#include <rtc_base/thread.h>
#include <rtc_base/async_packet_socket.h>

#include "ice/base/linux/event_loop.h"
#include "ice/base/linux/physical_socket_ex.h"

namespace ice { 

class UdpPacketData {
public:
    UdpPacketData(const char* data, size_t size, const rtc::SocketAddress& addr) :
        data_(new char[size]),
        size_(size),
        addr_(addr)
    {
        memcpy(data_, data, size);
    }
    
    ~UdpPacketData() {
        if (data_) {
            delete[] data_;
            data_ = nullptr;
        }
    }
    
    char* data() { return data_; }
    size_t size() { return size_; }
    const rtc::SocketAddress& addr() { return addr_; }

private:
    char* data_;
    size_t size_;
    rtc::SocketAddress addr_;
};

class AsyncUdpSocket : public rtc::AsyncPacketSocket {
public:
    AsyncUdpSocket(EventLoop* el, PhysicalSocketEx* socket);
    ~AsyncUdpSocket();
    
    rtc::SocketAddress GetLocalAddress() const override;
    rtc::SocketAddress GetRemoteAddress() const override;

    void RecvData();
    void SendData();
    
    int Send(const void*, size_t,
            const rtc::PacketOptions&) override { return -1; }
    int SendTo(const void* data, size_t size, const rtc::SocketAddress& addr,
            const rtc::PacketOptions& options) override;

    int Close() override;
    
    State GetState() const override;
    int GetOption(rtc::Socket::Option opt, int* value) override;
    int SetOption(rtc::Socket::Option opt, int value) override;
    int GetError() const override;
    void SetError(int error) override;

private:
    int AddUdpPacket(const char* data, size_t size, const rtc::SocketAddress& addr);

private:
    EventLoop* el_;
    IOWatcher* socket_watcher_;
    std::unique_ptr<PhysicalSocketEx> psock_;
    char* buf_;
    size_t size_;

    std::list<UdpPacketData*> udp_packet_list_;
};

} // namespace ice


#endif  // ICE_BASE_LINUX_ASYNC_UDP_SOCKET_H_


