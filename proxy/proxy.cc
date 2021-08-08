#include <co_routine.h>

#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/TcpClient.h>
#include <muduo/base/Logging.h>

#include <BasicParser.h>
#include <muduo/base/Timestamp.h>

#include <message.pb.h>

#include "ProxyParser.h"
#include "ThreadEnvironment.h"

struct ThreadEnvironment;
using namespace muduo::net;
using muduo::Timestamp;
using chatServer::BasicParser;
using chatServer::RequestType;
using chatServer::Header;
using chatServer::ServerInfo;


std::pair<int, std::shared_ptr<TcpClient>> newChatClient(EventLoop* loop, const std::string& ip, int port) {
    auto                              server = std::make_shared<TcpClient>(loop, InetAddress(ip, port), "Chat Client");
    auto                              index = environment->getStamp();
    server->setMessageCallback([](auto, auto buffer, auto) {
        environment->onChatServerMessage(buffer);
    });
    server->connect();
    return {index, server};
}

void redis_init_subscribe(EventLoop* loop) {
    auto ret = environment->redis_client.command([loop](auto, redisReply* reply) {
        if (reply->type == REDIS_REPLY_ARRAY && reply->elements == 3) {
            if (reply->element[2]->type == REDIS_REPLY_STRING) {
                std::string serverInfo(reply->element[2]->str, reply->element[2]->len);
                ServerInfo info;
                info.ParseFromString(serverInfo);
                LOG_DEBUG << "got new ip address: " << info.DebugString();
                auto area = info.area();

                auto [index, server] = newChatClient(loop, info.ip(), info.port());
                if (area == 1) {
                    server->setConnectionCallback([i = index](auto conn) {
                        if (conn->disconnected()) {
                            environment->chat_server_area_1.reset();
                            LOG_INFO << "chat server 1 is down";
                        }
                    });
                    environment->chat_server_area_1 = server;
                    LOG_INFO << "chat server 1 is up";
                }
                else {
                    server->setConnectionCallback([i = index](auto conn) {
                        if (conn->disconnected()) {
                            environment->chat_server_area_2.reset();
                            LOG_INFO << "chat server 2 is down";
                        }
                    });
                    environment->chat_server_area_2 = server;
                    LOG_INFO << "chat server 2 is up";
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

int main() {
    muduo::g_logLevel = muduo::Logger::ERROR;

    EventLoop loop;
    TcpServer server(&loop, InetAddress(23456), "proxy");
    // each thread will alloc a client to database and chat server

    std::vector<EventLoop*> thread_loops;

    server.setThreadNum(6);
    

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
            auto chat_server = environment->user_info[header.uid] == 1
                                   ? environment->chat_server_area_1
                                   : environment->chat_server_area_2;
            chat_server->connection()->send(str, len);
        }
    });

    server.setMessageCallback([](const TcpConnectionPtr& connptr, Buffer* buffer, auto) {
        auto parser = boost::any_cast<ProxyParser>(connptr->getMutableContext());
        parser->parse(buffer);
    });

    LOG_INFO << "thread loops size is " << thread_loops.size();


    server.setThreadInitCallback([&thread_loops](auto* loop_ptr) {
        thread_loops.push_back(loop_ptr);
        environment = new ThreadEnvironment(loop_ptr);

        loop_ptr->runAfter(1, [loop_ptr]() {
            redis_init_subscribe(loop_ptr);
            });
    });

    server.start();
    loop.loop();
}
