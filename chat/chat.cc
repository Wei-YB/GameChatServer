#include <list>
#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include "message.pb.h"

namespace chatServer::chat {
using chatServer::Message;
using muduo::net::TcpConnectionPtr;
using muduo::net::TcpServer;
using muduo::net::EventLoop;

using MessageInfo = std::pair<Message, std::string>;
using MessageInfoPtr = std::shared_ptr<MessageInfo>;

class MessageQueue {
public:
    void push(std::shared_ptr<MessageInfoPtr> info);
    void push(Message& message);

    decltype(auto) pop() {
        auto ptr =  message_list_.front();
        message_list_.pop_front();
        return ptr;
    }

    void shutdown() {
        // never clear a message_list
        // TODO: write message to offline server
        message_list_.clear();
    }

    std::list<MessageInfoPtr> message_list_;
};

// run with a tick to clear the message buffer
// not really client, hold a connection to proxy
class Client {

    void disconnect() {
        // release the pointer with connection
    }

    void handleMessage() {
        
    }

private:
    std::unordered_set<int> black_list_;
    TcpConnectionPtr ptr;
    MessageQueue queue_;
};
}

using namespace  chatServer::chat;

int main() {
    EventLoop main_loop;
    TcpServer server(&main_loop, muduo::net::InetAddress(50050), "ChatServer");

    server.setThreadInitCallback([](auto) {});

    // each connection means a client online, but not login!, we don't know who is using this client 
    server.setConnectionCallback([](auto) {});

    server.setMessageCallback([](auto, auto, auto) {});
}