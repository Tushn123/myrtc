/***************************************************************************
 * 
 * Copyright (c) 2022 str2num.com, Inc. All Rights Reserved
 * $Id$ 
 * 
 **************************************************************************/
 
 
 
/**
 * @file udp_port.cpp
 * @author str2num
 * @version $Revision$ 
 * @brief 
 *  
 **/

#include "ice/udp_port.h"

#include <sstream>

#include <rtc_base/logging.h>
#include <rtc_base/string_encode.h>

#include "ice/ice_connection.h"

namespace ice {

#if defined(ICE_POSIX)

UDPPort::UDPPort(EventLoop* el,
        const std::string& transport_name,
        int component,
        IceParameters ice_params) :
    el_(el),
    transport_name_(transport_name),
    component_(component),
    ice_params_(ice_params)
{
}

#elif defined(ICE_WIN)
UDPPort::UDPPort(rtc::Thread* network_thread,
    const std::string& transport_name,
    int component,
    IceParameters ice_params) :
    network_thread_(network_thread),
    transport_name_(transport_name),
    component_(component),
    ice_params_(ice_params)
{
}
#endif

UDPPort::~UDPPort() {
}

int UDPPort::BindSocket(rtc::Socket* socket,
		const rtc::SocketAddress& local_address,
		uint16_t min_port,
		uint16_t max_port) 
{
	int ret = -1;
	if (min_port == 0 && max_port == 0) {
		// If there's no port range, let the OS pick a port for us.
		ret = socket->Bind(local_address);
	} else {
		// Otherwise, try to find a port in the provided range.
		for (int port = min_port; ret < 0 && port <= max_port; ++port) {
			ret = socket->Bind(rtc::SocketAddress(local_address.ipaddr(), port));
		}
	}
	return ret;
}

int UDPPort::CreateIceCandidate(Network* network, int min_port, int max_port,
        Candidate& c) 
{
    rtc::Socket* socket = nullptr;
#if defined(ICE_POSIX)
    PhysicalSocketEx* psock = new PhysicalSocketEx();
    if (!psock->Create(network->ip().family(), SOCK_DGRAM)) {
        RTC_LOG(LS_ERROR) << "Create socket failed with error " << socket->GetError();
        delete psock;
        return -1;
    }
    
    socket = psock;

#elif defined(ICE_WIN)
    socket = network_thread_->socketserver()->CreateSocket(network->ip().family(), SOCK_DGRAM);
    if (!socket) {
        RTC_LOG(LS_ERROR) << "Create socket failed";
        return -1;
    }
#endif
    	
    if (BindSocket(socket, rtc::SocketAddress(network->ip(), 0), min_port, max_port) != 0) {
        RTC_LOG(LS_ERROR) << "UDP bind failed with error " << socket->GetError();
        delete socket;
        return -1;
    }    
 
    local_addr_ = socket->GetLocalAddress();
    
#if defined(ICE_POSIX)
    async_socket_ = std::make_unique<AsyncUdpSocket>(el_, psock);
#elif defined(ICE_WIN)
    async_socket_ = std::make_unique<rtc::AsyncUDPSocket>(socket);
#endif
    async_socket_->SignalReadPacket.connect(this, &UDPPort::OnReadPacket);

    RTC_LOG(LS_INFO) << "prepared socket address " << local_addr_.ToString();
    
    c.component = component_;
    c.protocol = "udp";
    c.address = local_addr_;
    c.port = local_addr_.port();
    c.priority = c.GetPriority(ICE_TYPE_PREFERENCE_HOST, 0, 0);
    c.username = ice_params_.ice_ufrag;
    c.password = ice_params_.ice_pwd;
    c.type = LOCAL_PORT_TYPE;
    c.foundation = Candidate::ComputeFoundation(c.type, c.protocol, "", c.address);
    
    candidates_.push_back(c);

    return 0;
}

IceConnection* UDPPort::CreateConnection(const Candidate& remote_candidate) {
#if defined(ICE_POSIX)
    IceConnection* conn = new IceConnection(el_, this, remote_candidate);
#elif defined(ICE_WIN)
    IceConnection* conn = new IceConnection(network_thread_, this, remote_candidate);
#endif
    auto ret = connections_.insert(
            std::make_pair(conn->remote_candidate().address, conn));
    if (ret.second == false && ret.first->second != conn) {
        RTC_LOG(LS_WARNING) << ToString() << ": create ice connection on "
            << "an existing remote address, addr: " 
            << conn->remote_candidate().address.ToString();
        ret.first->second->Destroy();
        ret.first->second = conn;
    }

    return conn;
}

IceConnection* UDPPort::GetConnection(const rtc::SocketAddress& addr) {
    auto iter = connections_.find(addr);
    return iter == connections_.end() ? nullptr : iter->second;
}

int UDPPort::SendTo(const char* buf, size_t len, const rtc::SocketAddress& addr) {
    if (!async_socket_) {
        return -1;
    }

    rtc::PacketOptions options;
    return async_socket_->SendTo(buf, len, addr, options);
}

void UDPPort::OnReadPacket(rtc::AsyncPacketSocket* /*socket*/, const char* buf, size_t size,
        const rtc::SocketAddress& addr, const int64_t& ts)
{
    if (IceConnection* conn = GetConnection(addr)) {
        conn->OnReadPacket(buf, size, ts);
        return;
    }

    std::unique_ptr<StunMessage> stun_msg;
    std::string remote_ufrag;
    bool res = GetStunMessage(buf, size, addr, &stun_msg, &remote_ufrag);
    if (!res || !stun_msg) {
        return;
    }
    
    if (STUN_BINDING_REQUEST == stun_msg->type()) {
        RTC_LOG(LS_INFO) << ToString() << ": Received "
            << StunMethodToString(stun_msg->type())
            << " id=" << rtc::hex_encode(stun_msg->transaction_id())
            << " from " << addr.ToString();
        SignalUnknownAddress(this, addr, stun_msg.get(), remote_ufrag);
    }
}

bool UDPPort::GetStunMessage(const char* data, size_t len,
        const rtc::SocketAddress& addr,
        std::unique_ptr<StunMessage>* out_msg,
        std::string* out_username)
{
    if (!StunMessage::ValidateFingerprint(data, len)) {
        return false;
    }
    
    out_username->clear();

    std::unique_ptr<StunMessage> stun_msg = std::make_unique<StunMessage>();
    rtc::ByteBufferReader buf(data, len);
    if (!stun_msg->Read(&buf) || buf.Length() != 0) {
        return false;
    }
    
    if (STUN_BINDING_REQUEST == stun_msg->type()) {
        if (!stun_msg->GetByteString(STUN_ATTR_USERNAME) ||
                !stun_msg->GetByteString(STUN_ATTR_MESSAGE_INTEGRITY))
        {
            RTC_LOG(LS_WARNING) << ToString() << ": recevied "
                << StunMethodToString(stun_msg->type())
                << " without username/M-I from "
                << addr.ToString();
            SendBindingErrorResponse(stun_msg.get(), addr, STUN_ERROR_BAD_REQUEST,
                    STUN_ERROR_REASON_BAD_REQUEST);
            return true;
        }

        std::string local_ufrag;
        std::string remote_ufrag;
        if (!ParseStunUserName(stun_msg.get(), &local_ufrag, &remote_ufrag) ||
                local_ufrag != ice_params_.ice_ufrag)
        {
            RTC_LOG(LS_WARNING) << ToString() << ": recevied "
                << StunMethodToString(stun_msg->type())
                << " with bad local_ufrag: " << local_ufrag
                << " from " << addr.ToString();
            SendBindingErrorResponse(stun_msg.get(), addr, STUN_ERROR_UNAUTHORIZED,
                    STUN_ERROR_REASON_UNAUTHORIZED);
            return true;
        }

        if (stun_msg->ValidateMessageIntegrity(ice_params_.ice_pwd) !=
                StunMessage::IntegrityStatus::kIntegrityOk)
        {
            RTC_LOG(LS_WARNING) << ToString() << ": recevied "
                << StunMethodToString(stun_msg->type())
                << " with bad M-I from "
                << addr.ToString();
            SendBindingErrorResponse(stun_msg.get(), addr, STUN_ERROR_UNAUTHORIZED,
                    STUN_ERROR_REASON_UNAUTHORIZED);
            return true;
        }

        *out_username = remote_ufrag;
    }
    
    *out_msg = std::move(stun_msg);
    return true;
}

bool UDPPort::ParseStunUserName(StunMessage* stun_msg, std::string* local_ufrag,
        std::string* remote_ufrag)
{
    local_ufrag->clear();
    remote_ufrag->clear();

    const StunByteStringAttribute* attr = stun_msg->GetByteString(STUN_ATTR_USERNAME);
    if (!attr) {
        return false;
    }

    //RFRAG:LFRAG
    std::string username = attr->GetString();
    std::vector<std::string> fields;
    rtc::split(username, ':', &fields);
    if (fields.size() != 2) {
        return false;
    }

    *local_ufrag = fields[0];
    *remote_ufrag = fields[1];
    return true;
}

std::string UDPPort::ToString() {
    std::stringstream ss;
    ss << "Port[" << this << ":" << transport_name_ << ":" << component_
        << ":" << ice_params_.ice_ufrag << ":" << ice_params_.ice_pwd
        << ":" << local_addr_.ToString() << "]";
    return ss.str();
}

void UDPPort::SendBindingErrorResponse(StunMessage* stun_msg,
        const rtc::SocketAddress& addr,
        int err_code,
        const std::string& reason)
{
     if (!async_socket_) {
        return;
    }

    StunMessage response;
    response.set_type(STUN_BINDING_ERROR_RESPONSE);
    response.set_transaction_id(stun_msg->transaction_id());
    auto error_attr = StunAttribute::CreateErrorCode();
    error_attr->set_code(err_code);
    error_attr->set_reason(reason);
    response.AddAttribute(std::move(error_attr));

    if (err_code != STUN_ERROR_BAD_REQUEST && err_code != STUN_ERROR_UNAUTHORIZED) {
        response.AddMessageIntegrity(ice_params_.ice_pwd);
    }

    response.AddFingerprint();

    rtc::ByteBufferWriter buf;
    if (!response.Write(&buf)) {
        return;
    }

    rtc::PacketOptions options;
    int ret = async_socket_->SendTo(buf.Data(), buf.Length(), addr, options);
    if (ret < 0) {
        RTC_LOG(LS_WARNING) << ToString() << " send "
            << StunMethodToString(response.type())
            << " error, ret=" << ret
            << ", to=" << addr.ToString();
    } else {
        RTC_LOG(LS_WARNING) << ToString() << " send "
            << StunMethodToString(response.type())
            << ", reason=" << reason
            << ", to=" << addr.ToString();
    }
}

void UDPPort::CreateStunUserName(const std::string& remote_username, 
        std::string* stun_attr_username)
{
    stun_attr_username->clear();
    *stun_attr_username = remote_username;
    stun_attr_username->append(":");
    stun_attr_username->append(ice_params_.ice_ufrag);
}

} // namespace ice


