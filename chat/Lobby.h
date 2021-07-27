#pragma once

#include <message.pb.h>
#include <muduo/net/TcpClient.h>


namespace chatServer::chat {

using chatServer::Message;
using muduo::net::TcpClient;

class Lobby {
public:
    void login(int uid, Client* client) {
        local_clients_.insert({uid, client});
    }

    void logout(int uid) {
        local_clients_.erase(uid);
    }

    void newMessage(std::shared_ptr<Message>&& msg);
    void handleMessage();

private:
    void broadcastMessage(std::shared_ptr<Message>&& msg) {
        // TODO: finish the broadcast message
    }

    void privateChatMessage(std::shared_ptr<Message>&& msg) {
        auto receiver = msg->receiver();
        if (local_clients_.count(receiver)) {
            // the client is on current server, just forward to target
            auto target_client = local_clients_[receiver];
            target_client->insert(std::move(msg));
            active_clients_.insert(target_client);
        }
        // TODO: Distributed chat service
        // else if(other_clients_.count(receiver)){
        //     auto target_server = other_clients_[receiver];
        //     // find the server where the client is in
        // }else {
        //     // this is an offline message
        //     // TODO: database_client_.write(msg);
        // }
    }
    

    TcpClient* database_client_ = nullptr;
    std::unordered_set<Client*> active_clients_;
    std::unordered_map<int, TcpClient*> other_clients_;
    std::unordered_map<int, Client*> local_clients_;
};

inline void Lobby::newMessage(std::shared_ptr<Message>&& msg) {
    if (msg->type() == Message_MessageType_LOBBY_BROADCAST || msg->type() == Message_MessageType_SERVER_BROADCAST) {
        broadcastMessage(std::move(msg));
    }
    else {
        privateChatMessage(std::move(msg));
    }
}

inline void Lobby::handleMessage() {
    for(auto client : active_clients_) {
        client->handleMessageQueue();
    }
    active_clients_.clear();
}


}
