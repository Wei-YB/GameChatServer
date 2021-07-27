#pragma once
#include <muduo/net/InetAddress.h>
#include <muduo/net/TcpConnection.h>
#include <muduo/net/TcpClient.h>
#include "Header.h"
#include "muduo/base/Logging.h"

using muduo::net::InetAddress;
using muduo::net::EventLoop;
using muduo::net::TcpConnectionPtr;
using muduo::net::TcpConnection;
using muduo::net::Buffer;
using muduo::net::TcpClient;
using muduo::Timestamp;
using chatServer::Header;

struct ThreadEnvironment {
    explicit ThreadEnvironment(EventLoop* loop);

    void onDataServerMessage(Buffer* buffer);

    void onChatServerMessage(Buffer* buffer);

    TcpClient data_server;
    TcpClient chat_server;

    std::unordered_map<int, std::weak_ptr<TcpConnection>>                 login_clients;
    std::unordered_map<int, std::pair<int, std::weak_ptr<TcpConnection>>> login_requests;
    std::unordered_map<int, std::pair<int, std::weak_ptr<TcpConnection>>> sign_up_requests;

    int getStamp();

private:
    int current_stamp_;
};

extern  thread_local ThreadEnvironment* environment;