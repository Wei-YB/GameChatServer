#include <list>
#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <muduo/base/Logging.h>
#include <HiRedis.h>
#include "ProxyConnection.h"
#include "BasicParser.h"
#include "Header.h"
#include "message.pb.h"
#include "Lobby.h"


using namespace chatServer::chat;


chatServer::ServerInfo current_server;

std::string current_server_str;

int main(int argc, char* argv[]) {

    if(argc != 3) {
        std::cout << "usage: ChatServer [area] [port]" << std::endl;
        return 0;
    }

    auto area = atoi(argv[1]);
    current_server.set_area(area);
    current_server.set_port(atoi(argv[2]));
    current_server.set_ip("127.0.0.1");
    current_server.set_type(chatServer::ServerInfo_ServerType_CHAT_SERVER);

    current_server_str = current_server.SerializeAsString();

    muduo::g_logLevel = muduo::Logger::DEBUG;
    
    EventLoop main_loop;
    TcpServer server(&main_loop, muduo::net::InetAddress(current_server.port()), "ChatServer");

    redis_client = new Hiredis(&main_loop, muduo::net::InetAddress("127.0.0.1", 6379));
    redis_client->connect();

    Lobby chat_lobby;

    server.setThreadInitCallback([](auto) {
    });

    // each connection means a client online, but not login!, we don't know who is using this client 
    server.setConnectionCallback([&chat_lobby](const TcpConnectionPtr& connPtr) {
        connPtr->setContext(ProxyConnection(connPtr, chat_lobby));
    });

    server.setMessageCallback([](const TcpConnectionPtr& conn_ptr, auto buffer, auto) {
        LOG_INFO << " get message from client";
        boost::any_cast<ProxyConnection>(conn_ptr->getMutableContext())->parse(buffer);
    });


    // TODO: send message to every client
    server.getLoop()->runEvery(0.05, [&chat_lobby]() {
        chat_lobby.handleMessage();
    });

    // TODO: need to fix lots of bugs
    main_loop.runAfter(1, [&]() {
        chat_lobby.start(&main_loop);});

    LOG_INFO << "chat server start on " << server.ipPort();
    server.start();
    main_loop.loop();
    return 0;
}
