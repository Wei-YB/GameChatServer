#pragma once
#include <muduo/net/InetAddress.h>
#include <muduo/net/TcpConnection.h>
#include <muduo/net/TcpClient.h>
#include <HiRedis.h>
#include <unordered_set>
#include <map>
#include "Header.h"
#include "muduo/base/Logging.h"

using muduo::net::InetAddress;
using muduo::net::EventLoop;
using muduo::net::TcpConnectionPtr;
using muduo::net::TcpConnection;
using muduo::net::Buffer;
using muduo::net::TcpClient;
using muduo::Timestamp;

using hiredis::Hiredis;

using chatServer::Header;

// struct ClientGroup {
//     void insert(int index, std::shared_ptr<TcpClient> client) {
//         clients_.insert({ index, client });
//     }
//
//     void erase(int index) {
//         clients_.erase(index);
//     }
//
//     bool exist(int index) const {
//         return clients_.count(index);
//     }
//
//     std::shared_ptr<TcpClient> next() {
//         if (clients_.empty())
//             return nullptr;
//         auto it = clients_.upper_bound(cur_index_);
//         if(it == clients_.end()) {
//             it = clients_.begin();
//         }
//         cur_index_ = it->first;
//         return it->second;
//     }
// private:
//     std::map<int, std::shared_ptr<TcpClient>> clients_;
//     int cur_index_ = 0;
// };

struct ThreadEnvironment {
    explicit ThreadEnvironment(EventLoop* loop);

    void onDataServerMessage(Buffer* buffer);

    void onChatServerMessage(Buffer* buffer);

    TcpClient data_server;

    // ClientGroup chat_server_area_1;
    // ClientGroup chat_server_area_2;

    std::shared_ptr<TcpClient> chat_server_area_1;
    std::shared_ptr<TcpClient> chat_server_area_2;

    Hiredis redis_client;

    std::unordered_map<int, int>                                                user_info;
    std::unordered_map<int, std::weak_ptr<TcpConnection>>                       login_clients;
    std::unordered_map<int, std::tuple<int, int, std::weak_ptr<TcpConnection>>> login_requests;
    std::unordered_map<int, std::pair<int, std::weak_ptr<TcpConnection>>>       sign_up_requests;

    int getStamp();

private:
    int current_stamp_;
};

extern  thread_local ThreadEnvironment* environment;