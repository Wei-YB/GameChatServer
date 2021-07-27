#pragma once

#include <message.pb.h>
#include <muduo/net/TcpClient.h>


namespace chatServer::chat {
class Client;

using chatServer::Message;
using muduo::net::TcpClient;



class Lobby {
public:
    void login(int uid, Client* client);

    void logout(int uid);

    void newMessage(std::shared_ptr<Message>&& msg);
    void handleMessage();

private:
    void broadcastMessage(std::shared_ptr<Message>&& msg);

    void privateChatMessage(std::shared_ptr<Message>&& msg);


    TcpClient* database_client_ = nullptr;
    std::unordered_set<Client*> active_clients_;
    std::unordered_map<int, TcpClient*> other_clients_;
    std::unordered_map<int, Client*> local_clients_;
};






}
