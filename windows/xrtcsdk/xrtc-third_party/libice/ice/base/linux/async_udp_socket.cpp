/***************************************************************************
 * 
 * Copyright (c) 2022 str2num.com, Inc. All Rights Reserved
 * $Id$ 
 * 
 **************************************************************************/
 
 
 
/**
 * @file async_udp_socket.cpp
 * @author str2num
 * @version $Revision$ 
 * @brief 
 *  
 **/

#include "ice/base/linux/async_udp_socket.h"

#include <rtc_base/logging.h>

namespace ice {

const size_t MAX_BUF_SIZE = 1500;

void AsyncUdpSocketIOCb(EventLoop* /*el*/, IOWatcher* /*w*/, 
        int /*fd*/, int events, void* data) 
{
    AsyncUdpSocket* udp_socket = (AsyncUdpSocket*)data;
    if ((int)EventLoop::EventType::READ & events) {
        udp_socket->RecvData();
    }

    if ((int)EventLoop::EventType::WRITE & events) {
        udp_socket->SendData();
    }
}

AsyncUdpSocket::AsyncUdpSocket(EventLoop* el, PhysicalSocketEx* psock) :
    el_(el),
    psock_(psock),
    buf_(new char[MAX_BUF_SIZE]),
    size_(MAX_BUF_SIZE)
{
    socket_watcher_ = el_->CreateIOEvent(AsyncUdpSocketIOCb, this);
    el_->StartIOEvent(socket_watcher_, psock_->socket(), (int)EventLoop::EventType::READ);
}

AsyncUdpSocket::~AsyncUdpSocket() {
    if (socket_watcher_) {
        el_->DeleteIOEvent(socket_watcher_);
        socket_watcher_ = nullptr;
    }

    if (buf_) {
        delete []buf_;
        buf_ = nullptr;
    }
}

rtc::SocketAddress AsyncUdpSocket::GetLocalAddress() const {
    return psock_->GetLocalAddress();
}

rtc::SocketAddress AsyncUdpSocket::GetRemoteAddress() const {
    return psock_->GetRemoteAddress();
}

void AsyncUdpSocket::RecvData() {
    while (true) { 
        rtc::SocketAddress remote_addr;
        int64_t ts = 0;
        int len = psock_->RecvFrom(buf_, size_, &remote_addr, &ts);
        if (len <= 0) {
            return;
        }
         
        SignalReadPacket(this, buf_, len, remote_addr, ts);
    }
}

inline bool IsBlockingError(int e) {
    return (e == EWOULDBLOCK) || (e == EAGAIN) || (e == EINPROGRESS);
}

void AsyncUdpSocket::SendData() {
    int sent = 0;
    while (!udp_packet_list_.empty()) {
        // 发送udp packet
        UdpPacketData* packet = udp_packet_list_.front();
        sent = psock_->SendTo(packet->data(), packet->size(), packet->addr());
        if (sent < 0 && IsBlockingError(psock_->GetError())) {
            sent = 0;
        }
        
        if (sent < 0) {
            RTC_LOG(LS_WARNING) << "send udp packet error, remote_addr: " <<
                packet->addr().ToString();
            delete packet;
            udp_packet_list_.pop_front();
            return;
        } else if (0 == sent) {
            RTC_LOG(LS_WARNING) << "send 0 bytes, try again, remote_addr: " <<
                packet->addr().ToString();
            return;
        } else {
            delete packet;
            udp_packet_list_.pop_front();
        }
    }

    if (udp_packet_list_.empty()) {
        el_->StopIOEvent(socket_watcher_, psock_->socket(), (int)EventLoop::EventType::WRITE);
    }
}

int AsyncUdpSocket::SendTo(const void* data, size_t size, const rtc::SocketAddress& addr,
        const rtc::PacketOptions&) 
{
    return AddUdpPacket((const char*)data, size, addr);
}

int AsyncUdpSocket::Close() {
    return psock_->Close();
}

AsyncUdpSocket::State AsyncUdpSocket::GetState() const {
	return STATE_BOUND;
}

int AsyncUdpSocket::GetOption(rtc::Socket::Option opt, int* value) {
	return psock_->GetOption(opt, value);
}

int AsyncUdpSocket::SetOption(rtc::Socket::Option opt, int value) {
	return psock_->SetOption(opt, value);
}

int AsyncUdpSocket::GetError() const {
	return psock_->GetError();
}

void AsyncUdpSocket::SetError(int error) {
	return psock_->SetError(error);
}

int AsyncUdpSocket::AddUdpPacket(const char* data, size_t size,
        const rtc::SocketAddress& addr)
{
    // 尝试发送list里面的数据
    int sent = 0;
    while (!udp_packet_list_.empty()) {
        // 发送udp packet
        UdpPacketData* packet = udp_packet_list_.front();
        sent = psock_->SendTo(packet->data(), packet->size(), packet->addr());
        if (sent < 0 && IsBlockingError(psock_->GetError())) {
            sent = 0;
        }

        if (sent < 0) {
            RTC_LOG(LS_WARNING) << "send udp packet error, remote_addr: " <<
                packet->addr().ToString();
            delete packet;
            udp_packet_list_.pop_front();
            return -1;
        } else if (0 == sent) {
            RTC_LOG(LS_WARNING) << "send 0 bytes, try again, remote_addr: " <<
                packet->addr().ToString();
            goto SEND_AGAIN;
        } else {
            delete packet;
            udp_packet_list_.pop_front();
        }
    }

    // 发送当前数据
    sent = psock_->SendTo(data, size, addr);
    if (sent < 0 && IsBlockingError(psock_->GetError())) {
        sent = 0;
    }

    if (sent < 0) {
        RTC_LOG(LS_WARNING) << "send udp packet error, remote_addr: " << addr.ToString();
        return -1;
    } else if (0 == sent) {
        RTC_LOG(LS_WARNING) << "send 0 bytes, try again, remote_addr: " << addr.ToString();
        goto SEND_AGAIN;
    } 
    
    return sent;

SEND_AGAIN: 
    UdpPacketData* packet_data = new UdpPacketData(data, size, addr);
    udp_packet_list_.push_back(packet_data);
    el_->StartIOEvent(socket_watcher_, psock_->socket(), (int)EventLoop::EventType::WRITE);

    return size;
}

} // namespace ice
