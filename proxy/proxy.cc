#include <co_routine.h>

#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/TcpClient.h>
#include <muduo/base/Logging.h>

#include <BasicParser.h>
#include <muduo/base/Timestamp.h>

#include "ProxyParser.h"
#include "ThreadEnvironment.h"

struct ThreadEnvironment;
using namespace muduo::net;
using muduo::Timestamp;
using chatServer::BasicParser;
using chatServer::RequestType;
using chatServer::Header;

int main() {
    EventLoop loop;
    TcpServer server(&loop, InetAddress(23456), "proxy");
    // each thread will alloc a client to database and chat server

    server.setThreadNum(0);
    server.setThreadInitCallback([](auto* loop) {
        environment = new ThreadEnvironment(loop);
    });

    server.setConnectionCallback([](const TcpConnectionPtr& connPtr) {
        if (connPtr->connected())
            connPtr->setContext(ProxyParser(connPtr));
        else {
            
            auto parser = boost::any_cast<ProxyParser>(connPtr->getMutableContext());
            if (parser->getState() != ClientState::LOGIN)
                return;
            Header header;
            header.request_type = static_cast<int>(RequestType::LOGOUT);
            header.uid = parser->player_uid;
            environment->login_clients.erase(header.uid);
            LOG_INFO << "player " << header.uid << " break connection with proxy";

            auto [str, len] = formatMessage(header);
            LOG_INFO << "send request to server with head: " << header.toString();
            environment->chat_server.connection()->send(str, len);
        }
    });

    server.setMessageCallback([](const TcpConnectionPtr& connptr, Buffer* buffer, auto) {
        auto parser = boost::any_cast<ProxyParser>(connptr->getMutableContext());
        parser->parse(buffer);
        });

    muduo::g_logLevel = muduo::Logger::DEBUG;
    
    server.start();
    loop.loop();
}
