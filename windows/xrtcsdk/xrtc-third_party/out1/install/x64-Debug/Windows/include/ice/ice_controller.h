/***************************************************************************
 * 
 * Copyright (c) 2022 ice.com, Inc. All Rights Reserved
 * $Id$ 
 * 
 **************************************************************************/
 
 
 
/**
 * @file ice_controller.h
 * @version $Revision$ 
 * @brief 
 *  
 **/



#ifndef  ICE_ICE_CONTROLLER_H_
#define  ICE_ICE_CONTROLLER_H_

#include <set>
#include <string>
#include <vector>

#include "ice/ice_connection.h"

namespace ice {

class IceTransportChannel;

struct IceControllerEvent {
    enum Type {
        REMOTE_CANDIDATE_GENERATION_CHANGE,
        NEW_CONNECTION_FROM_LOCAL_CANDIDATE,
        NEW_CONNECTION_FROM_REMOTE_CANDIDATE,
        NEW_CONNECTION_FROM_UNKNOWN_REMOTE_ADDRESS,
        CONNECT_STATE_CHANGE,
        SELECTED_CONNECTION_DESTROYED,
    };

    IceControllerEvent(const Type& _type)  // NOLINT: runtime/explicit
        : type(_type) {}

    std::string ToString() const {
        std::string reason;
        switch (type) {
        case REMOTE_CANDIDATE_GENERATION_CHANGE:
            reason = "remote candidate generation maybe changed";
            break;
        case NEW_CONNECTION_FROM_LOCAL_CANDIDATE:
            reason = "new candidate pairs created from a new local candidate";
            break;
        case NEW_CONNECTION_FROM_REMOTE_CANDIDATE:
            reason = "new candidate pairs created from a new remote candidate";
            break;
        case NEW_CONNECTION_FROM_UNKNOWN_REMOTE_ADDRESS:
            reason = "a new candidate pair created from an unknown remote address";
            break;
        case CONNECT_STATE_CHANGE:
            reason = "candidate pair state changed";
            break;
        case SELECTED_CONNECTION_DESTROYED:
            reason = "selected candidate pair destroyed";
            break;
        }

        return reason;
    }

    Type type;
};

struct PingResult {
    PingResult(const IceConnection* conn, int ping_interval) :
        conn(conn), ping_interval(ping_interval) {}

    const IceConnection* conn = nullptr;
    int ping_interval = 0;
};

class IceController {
public:
    IceController(IceTransportChannel* ice_channel) : ice_channel_(ice_channel) {}
    ~IceController() = default;
  
    void AddConnection(IceConnection* conn);
    const std::vector<IceConnection*> connections() { return connections_; }
    bool HasPingableConnection();
    PingResult SelectConnectionToPing(int64_t last_ping_sent_ms);
    IceConnection* SortAndSwitchConnection();
    void SetSelectedConnection(IceConnection* conn) { selected_connection_ = conn; }
    void MarkConnectionPinged(IceConnection* conn);
    bool ReadyToSend(IceConnection* conn);
    void OnConnectionDestroyed(IceConnection* conn);

private:
    bool IsPingable(IceConnection* conn, int64_t now);
    const IceConnection* FindNextPingableConnection(int64_t last_ping_sent_ms);
    bool IsConnectionPastPingInterval(const IceConnection* conn, int64_t now);
    int GetConnectionPingInterval(const IceConnection* conn, int64_t now);
    
    bool weak() {
        return selected_connection_ == nullptr ||  selected_connection_->weak();
    }

    bool MorePingable(IceConnection* conn1, IceConnection* conn2); 
    int CompareConnections(IceConnection* a, IceConnection* b);

private:
    IceTransportChannel* ice_channel_;
    IceConnection* selected_connection_ = nullptr;
    std::vector<IceConnection*> connections_;
    std::set<IceConnection*> unpinged_connections_;
    std::set<IceConnection*> pinged_connections_;
};

} // namespace ice

#endif  // ICE_ICE_CONTROLLER_H_


