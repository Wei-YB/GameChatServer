#include "HiRedis.h"

#include "muduo/net/TcpServer.h"
#include <muduo/net/EventLoop.h>

using hiredis::Hiredis;
using namespace  muduo::net;

class RequestParser {
    void parse(const TcpConnectionPtr& connection, Buffer* buffer) {
        if(state_ == State::WAIT_LENGTH && buffer->readableBytes() > sizeof(int)) {
            data_length_ = buffer->readInt32();
            state_ = State::WAIT_TYPE;
        }
        if(state_ == State::WAIT_TYPE && buffer->readableBytes() > sizeof(int)) {
            
        }
    }

private:
    enum class State {
        WAIT_LENGTH,
        WAIT_TYPE,
        WAIT_UID,
        WAIT_DATA,
    };
    int data_length_;
    int request_type;
    uint64_t uid;
    State state_;
};


int main() {
    EventLoop loop;
    Hiredis client(&loop, InetAddress("127.0.0.1", 6379));
    client.connect();
    TcpServer server(&loop, InetAddress(12345), "Database");

    

    server.setThreadNum(8);
    server.start();
    loop.loop();

}