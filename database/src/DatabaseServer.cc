#include "HiRedis.h"

#include "Parser.h"

#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>

#include "Config.h"
#include "message.pb.h"
#include "muduo/base/Logging.h"
#include "muduo/base/AsyncLogging.h"

using hiredis::Hiredis;
using chatServer::PlayerInfo;
using chatServer::PlayerList;

using namespace muduo::net;
using namespace chatServer::database;


muduo::AsyncLogging* g_asyncLog = NULL;

void asyncOutput(const char* msg, int len)
{
    g_asyncLog->append(msg, len);
}

// how to create redis client for each loop
int main(int argc, char* argv[]) {
    daemon(1, 0);

    EventLoop loop;

    TcpServer                             server(&loop, InetAddress(12345), "Database");

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

    

    g_asyncLog = new muduo::AsyncLogging("Logger", 1024 * 1024, 1);
    g_asyncLog->start();
    muduo::Logger::setOutput(asyncOutput);


    server.start();
    loop.loop();

}
