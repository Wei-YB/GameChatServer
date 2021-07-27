#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpServer.h>
#include <muduo/net/TcpClient.h>
#include <message.pb.h>
#include <Header.h>

using namespace muduo::net;

int cnt = 0;

char messageBuffer[1024];
int messageLen;

std::chrono::time_point<std::chrono::system_clock> start_point;

void finishFunc() {
    static int finishClient = 0;
    finishClient++;
    auto endpoint = std::chrono::system_clock::now();
    auto time_cost = std::chrono::duration_cast<std::chrono::milliseconds>(endpoint - start_point);
    if(finishClient == 10) {
        std::cout << " finish the test with time = " << time_cost.count()<< std::endl;
        exit(0);
    }
}

int main() {
    EventLoop loop;

    chatServer::PlayerInfo info;
    info.set_nickname("owen");
    // info.set_stamp(0);
    auto message = info.SerializeAsString();
    chatServer::Header header;
    header.data_length = message.size();
    header.request_type = static_cast<int>(chatServer::RequestType::LOGIN);
    auto next = header.fill(messageBuffer);
    memcpy(next, message.c_str(), message.size());
    messageLen = header.request_length();

    std::vector<TcpClient*> clients;

    for (int i = 0; i < 10; ++i)
    {
        auto client = new TcpClient(&loop, InetAddress("127.0.0.1", 12345), "client");
        client->setConnectionCallback([](const TcpConnectionPtr& connPtr) {
            if (connPtr->connected())
            {
                int message_cnt = 0;
                connPtr->setContext(message_cnt);
                connPtr->send(messageBuffer, messageLen);
            }else if(connPtr->disconnected()) {
                // do nothing;
            }
        });
        client->setMessageCallback([](const TcpConnectionPtr& connPtr, Buffer* buffer, auto) {
            auto val = boost::any_cast<int>(connPtr->getMutableContext());
            buffer->retrieveAll();
            if(*val == 10000) {
                connPtr->shutdown();
                finishFunc();
                return;
            }
            connPtr->send(messageBuffer, messageLen);
            ++(*val);
        });
        client->connect();
    }


    start_point = std::chrono::system_clock::now();

    loop.loop();
}
