#pragma once

#include <muduo/net/TcpServer.h>

#include <message.pb.h>

#include <Util.h>

#include "HiRedis.h"

namespace chatServer::database {
using muduo::net::TcpConnectionPtr;
using muduo::net::Buffer;

extern thread_local hiredis::Hiredis* redis_client;

class Parser {
public:
    using ConnPtr = TcpConnectionPtr;

    Parser(const ConnPtr& connection);

    Parser(const Parser&) = default;
    Parser(Parser&&) = default;

    // TODO: add parser for blacklist information
    // TODO: add parser for online and offline info
    void parse(Buffer* buffer);

    void disConnect();


private:
    std::shared_ptr<PlayerInfo> getPlayerInfo(Buffer* buffer) const;

    void parseSignUp(std::shared_ptr<PlayerInfo> info);

    void parseLogin(std::shared_ptr<PlayerInfo> info);

    void parseInfo(std::shared_ptr<PlayerInfo> info);

    // TODO: info for the black list
    // void parseGetBlackList();
    // void parseAddBlackList();
    // void parseDelBlackList();

    void successResponse(std::shared_ptr<PlayerInfo> info);

    void failResponse(int status);

    // info: call this function only in bad status
    void sendResponse(int status);

    // info: call this function only in success status
    void sendResponse(int status, std::shared_ptr<PlayerInfo> info);


    enum class State {
        WAIT_LENGTH,
        WAIT_TYPE,
        WAIT_UID,
        WAIT_DATA,
    };

    TcpConnectionPtr conn_;

    char buffer_[1024];

    size_t      data_length_;
    RequestType request_type_;
    int         uid_;
    State       state_;
};
}
