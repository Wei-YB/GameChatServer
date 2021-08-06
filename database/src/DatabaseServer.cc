#include "HiRedis.h"

#include "Parser.h"

#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>

#include "Config.h"
#include "message.pb.h"
#include "muduo/base/Logging.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

using hiredis::Hiredis;
using chatServer::PlayerInfo;
using chatServer::PlayerList;

using namespace muduo::net;
using namespace chatServer::database;


// how to create redis client for each loop
int main(int argc, char* argv[]) {
    std::cout << "server will start... " << std::endl;
    std::cout << "going to daemon... " << std::endl;
    daemon(1, 0);

    auto logger = spdlog::basic_logger_mt("logger", "./log.txt");
    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::debug);
    spdlog::flush_on(spdlog::level::info);

    EventLoop loop;

    TcpServer server(&loop, InetAddress(12345), "Database");

    server.setThreadInitCallback([](auto* loop) {
        redis_client = new Hiredis(loop, InetAddress("127.0.0.1", 6379));
        redis_client->connect();
    });

    server.setConnectionCallback([](const TcpConnectionPtr& connPtr) {
        if (connPtr->connected()) {
            spdlog::info("new connection from {0}", connPtr->peerAddress().toIpPort());
            // LOG_INFO << "new connection from " << connPtr->peerAddress().toIpPort();
            connPtr->setContext(Parser(connPtr));
        }
        else if (connPtr->disconnected()) {
            boost::any_cast<Parser>(connPtr->getContext()).disConnect();
            spdlog::info("connection break with {0}", connPtr->peerAddress().toIpPort());
        }
    });

    server.setMessageCallback([](const TcpConnectionPtr& connPtr, Buffer* buffer, auto stamp) {
        auto parser = boost::any_cast<Parser>(connPtr->getMutableContext());
        parser->parse(buffer);
    });


    spdlog::info("server start at 0.0.0.0:12345");

    muduo::g_logLevel = muduo::Logger::ERROR;

    muduo::Logger::setOutput([](const char* msg, int len) {
        spdlog::error(std::string(msg, len));
    });

    // g_asyncLog = new muduo::AsyncLogging("Logger", 1024 * 1024, 1);
    // g_asyncLog->start();
    // muduo::Logger::setOutput(asyncOutput);

    server.start();
    loop.loop();

}
