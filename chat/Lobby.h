#pragma once

#include <list>
#include <message.pb.h>
#include <muduo/net/TcpClient.h>
#include <muduo/net/EventLoop.h>

#include "HiRedis.h"
#include "Player.h"

namespace chatServer::chat {
class ProxyConnection;

using chatServer::Message;
using muduo::net::TcpClient;


extern hiredis::Hiredis* redis_client;

// TODO: how to make the response?
class Lobby {
public:
    void login(std::shared_ptr<PlayerInfo> player_info, const TcpConnectionPtr& conn);

    void logout(int uid);

    void newMessage(std::shared_ptr<Message>&& msg);
    void handleMessage();

    void addBlackList(int uid, std::shared_ptr<PlayerInfo> info);
    void delBlackList(int uid, int black_player);
    std::shared_ptr<PlayerList> getBlackList(int uid);

    void newServerInfo(muduo::net::EventLoop* loop, const std::string& info);

    void start(muduo::net::EventLoop* loop);

private:
    int initBlackList(int uid, redisReply* reply);

    void broadcastMessage(std::shared_ptr<Message>&& msg);

    void privateChatMessage(std::shared_ptr<Message>&& msg);

    void forwardMessage(std::shared_ptr<Message> msg, std::string server);

    void offlineMessage(std::shared_ptr<Message>&& msg);

    int handleOfflineMessage(int uid, hiredis::Hiredis* client, redisReply* reply);

    std::string formatBlackList(const std::unordered_map<int, std::string>& black_list) const {
        std::string result;
        for(auto [uid, name] : black_list) {
            result += std::to_string(uid);
            result += ' ';
            result += name;
            result += ' ';
        }
        result.pop_back();
        return result;
    }

    std::shared_ptr<Message> onlineNotify(int uid);

    std::unordered_map<std::string, std::shared_ptr<TcpClient>> other_servers_;

    std::list<std::shared_ptr<Message>> broadcast_message_list_;

    std::unordered_map<int, std::string> player_cache_;
    std::unordered_map<int, std::shared_ptr<Player>> local_players_;
    std::unordered_set<std::shared_ptr<Player>>      active_players_;
};

}
