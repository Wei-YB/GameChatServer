#include <list>
#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <muduo/base/Logging.h>
#include <HiRedis.h>
#include "Client.h"
#include "BasicParser.h"
#include "Header.h"
#include "message.pb.h"
#include "Lobby.h"


using namespace chatServer::chat;


int main() {
    muduo::g_logLevel = muduo::Logger::DEBUG;
    
    EventLoop main_loop;
    TcpServer server(&main_loop, muduo::net::InetAddress(12346), "ChatServer");

    redis_client = new Hiredis(&main_loop, muduo::net::InetAddress("127.0.0.1", 6379));
    redis_client->connect();

    Lobby chat_lobby;

    server.setThreadInitCallback([](auto) {
    });

    // each connection means a client online, but not login!, we don't know who is using this client 
    server.setConnectionCallback([&chat_lobby](const TcpConnectionPtr& connPtr) {
        connPtr->setContext(Client(connPtr, chat_lobby));
    });

    server.setMessageCallback([](const TcpConnectionPtr& conn_ptr, auto buffer, auto) {
        LOG_INFO << " get message from client";
        boost::any_cast<Client>(conn_ptr->getMutableContext())->parse(buffer);
    });


    // TODO: send message to every client
    server.getLoop()->runEvery(0.05, [&chat_lobby]() {
        chat_lobby.handleMessage();
    });

    server.start();
    main_loop.loop();
    return 0;
}
