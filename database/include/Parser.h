#pragma once

#include <muduo/net/TcpServer.h>
#include <message.pb.h>
#include <BasicParser.h>
#include "HiRedis.h"

namespace chatServer::database {
using muduo::net::TcpConnectionPtr;
using muduo::net::Buffer;
using chatServer::RequestType;

extern thread_local hiredis::Hiredis* redis_client;


class Parser : public BasicParser {
public:
    using ConnPtr = TcpConnectionPtr;

    Parser(const ConnPtr& connection);

    void disConnect();
private:
    void parseData(Buffer* buffer) override;

    std::shared_ptr<PlayerInfo> getPlayerInfo(Buffer* buffer) const;

    void parseInfo(std::shared_ptr<PlayerInfo> info);

    void parseSignUp(std::shared_ptr<PlayerInfo> info);

    void parseLogin(std::shared_ptr<PlayerInfo> info);

    // no async so no need to copy header
    void parseBadHead(Buffer* buffer);

    void response(const std::pair<char*, size_t>& message) const;

    TcpConnectionPtr conn_;
};

}


