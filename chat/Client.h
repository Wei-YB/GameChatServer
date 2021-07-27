#pragma once
#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <muduo/base/Logging.h>
#include <HiRedis.h>
#include <list>

#include "BasicParser.h"
#include "Lobby.h"

#include "message.pb.h"

static const auto* login_cmd =
    "if(redis.call('EXISTS', KEYS[1]..':uid')==0)"
    "then return 0 end; "
    "local uid =  redis.call('GET', KEYS[1]..':uid'); "
    "return redis.call ('LRANGE', uid..':player', 0, -1);";


namespace chatServer::chat {
using chatServer::Message;
using muduo::net::TcpConnectionPtr;
using muduo::net::TcpServer;
using muduo::net::EventLoop;
using muduo::net::Buffer;
using hiredis::Hiredis;


extern Hiredis* redis_client;

class Lobby;


class Client : public BasicParser {
public:
    Client(const TcpConnectionPtr& conn_ptr, Lobby& lobby);

    ~Client() override {
        if (login_)
            lobby_.logout(uid_);
    }

    void parseData(Buffer* buffer) override;

    void disconnect();

    void handleMessage(Buffer* buffer);

    void handleLogin(Buffer* buffer);

    void handleChat(Buffer* buffer);

    // void handleChatMessage(std::shared_ptr<Message>&& msg);

    // void handleBroadcastMessage(std::shared_ptr<Message> msg);

    void handleMessageQueue();

    void insert(std::shared_ptr<Message>&& msg) {
        // TODO: fix this with r-value ref
        queue_.push_back(std::move(msg));
    }

private:
    void sendMessage(const std::pair<char*, size_t>& message) const;

    int                     uid_;
    Lobby&                  lobby_;
    std::unordered_set<int> black_list_;

    std::weak_ptr<muduo::net::TcpConnection> ptr_;
    std::list<std::shared_ptr<Message>>      queue_;
    bool                                     login_;
};
}
