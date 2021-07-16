#include <co_routine.h>

#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>
#include <muduo/base/Logging.h>


using namespace muduo::net;


class Parser {
public:
    void parse(Buffer* buffer) {
        if (buffer->readableBytes() >= 4)
            message_length_ = buffer->readInt32();

    }

    enum class State {
        WAITING_LENGTH,
        WAITING_TYPE,
        WAITING_UID,
        WAITING_DATA,
        FINISH
    };

private:
    int message_length_;
};

int main() {
    EventLoop loop;
    TcpServer server(&loop, InetAddress(12345), "proxy");

}
