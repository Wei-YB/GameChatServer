#include "HiRedis.h"

#include "Parser.h"

#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>

#include "message.pb.h"
#include "muduo/base/Logging.h"

using hiredis::Hiredis;
using chatServer::PlayerInfo;
using chatServer::BlackList;

using namespace muduo::net;
using namespace chatServer::database;


// how to create redis client for each loop
int main() {
    muduo::g_logLevel = muduo::Logger::DEBUG;
    EventLoop loop;
    Hiredis   client(&loop, InetAddress("127.0.0.1", 6379));
    client.connect();
    TcpServer server(&loop, InetAddress(12345), "Database");
    server.setThreadNum(8);

    server.setThreadInitCallback([](auto* loop) {
        redis_client = new Hiredis(loop, InetAddress("127.0.0.1", 6379));
        redis_client->connect();
    });

    server.setConnectionCallback([](const TcpConnectionPtr& connPtr) {
        if (connPtr->connected()) {
            LOG_INFO << "new connection from " << connPtr->peerAddress().toIpPort();
            connPtr->setContext(Parser(connPtr));
        }
        else if (connPtr->disconnected()) {
            boost::any_cast<Parser>(connPtr->getContext()).disConnect();
            LOG_INFO << "connection break with " << connPtr->peerAddress().toIpPort();
        }
    });

    server.setMessageCallback([](const TcpConnectionPtr& connPtr, Buffer* buffer, auto stamp) {
        auto parser = boost::any_cast<Parser>(connPtr->getMutableContext());
        parser->parse(buffer);
    });


    LOG_INFO << "server start at 0.0.0.0:12345";

    server.start();
    loop.loop();

}
