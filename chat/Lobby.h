#pragma once

#include <list>
#include <message.pb.h>
#include <muduo/net/TcpClient.h>

#include "HiRedis.h"
#include "Player.h"


namespace chatServer::chat {
class ProxyConnection;

using chatServer::Message;
using muduo::net::TcpClient;


extern hiredis::Hiredis* redis_client;

// TODO: change the local_clients with Player, rather then proxy client

class Lobby {
public:
    void login(std::shared_ptr<PlayerInfo> player_info, const TcpConnectionPtr& conn);

    void logout(int uid);

    void newMessage(std::shared_ptr<Message>&& msg);
    void handleMessage();

private:
    void broadcastMessage(std::shared_ptr<Message>&& msg);

    void privateChatMessage(std::shared_ptr<Message>&& msg);


    TcpClient* other_lobby_server_ = nullptr;
    int        current_area = 1;

    std::list<std::shared_ptr<Message>> broadcast_message_list_;

    // std::unordered_set<ProxyConnection*> active_clients_;
    // std::unordered_map<int, TcpClient*> other_clients_;
    // std::unordered_map<int, ProxyConnection*> local_clients_;

    std::unordered_map<int, std::shared_ptr<Player>> local_players_;
    std::unordered_set<std::shared_ptr<Player>>      active_players_;
};

}
