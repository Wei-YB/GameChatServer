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


std::shared_ptr<TcpClient> newChatClient(EventLoop* loop, const std::string& addr) {
    const auto dot_pos = addr.find(':');
    auto       ip = addr.substr(0, dot_pos);
    auto       port = std::stoi(addr.substr(dot_pos + 1));
    auto       server = std::make_shared<TcpClient>(loop, InetAddress(ip, port), "Chat Client");

    server->setMessageCallback([](auto, auto buffer, auto) {
        environment->onChatServerMessage(buffer);
    });

    server->setConnectionCallback([](const auto& conn) {
        if (conn->disconnected()) {
            environment->chat_server.reset();
        }

    });
    server->connect();
    return server;
}


struct ServerInfo {
    std::string addr;
    int area;
    int type;   // this is a proxy or database or chat
};

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
            environment->chat_server->connection()->send(str, len);
        }
    });

    server.setMessageCallback([](const TcpConnectionPtr& connptr, Buffer* buffer, auto) {
        auto parser = boost::any_cast<ProxyParser>(connptr->getMutableContext());
        parser->parse(buffer);
    });

    muduo::g_logLevel = muduo::Logger::DEBUG;

    loop.runAfter(1, [&loop]() {
                      auto ret = environment->redis_client.command([&loop](auto, redisReply* reply) {
                          if (reply->type == REDIS_REPLY_ARRAY && reply->elements == 3) {
                              if (reply->element[2]->type == REDIS_REPLY_STRING)
                              {
                                  std::string server(reply->element[2]->str, reply->element[2]->len);
                                  LOG_DEBUG << "got new ip address: " << server;
                                  if (!environment->chat_server) {
                                      LOG_DEBUG << "try to get new connection to chat server";
                                      environment->chat_server = newChatClient(&loop, server);
                                  }
                              }
                          }
                          else {
                              LOG_ERROR << "redis subscribe error";
                          }
                          return 1;
                      }, "SUBSCRIBE service_notify");
                      if (ret == REDIS_ERR) {
                          LOG_ERROR << "redis command error";
                          exit(0);
                      }
                  }
    );

    server.start();
    loop.loop();
}
