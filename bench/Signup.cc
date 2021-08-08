#include <Header.h>
#include <Request.h>
#include <muduo/net/TcpClient.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpServer.h>
#include <muduo/base/Timestamp.h>
#include <muduo/base/Logging.h>
using namespace std;
using namespace chatServer;
using namespace muduo::net;
using namespace muduo;


int main(int args, char* argv[]) {

    EventLoop loop;
    TcpClient client(&loop, InetAddress("127.0.0.1", 23456), "test");
    int index = 0;

    auto player_info = make_shared<PlayerInfo>();
    player_info->set_password("pass");

    client.setConnectionCallback([&](const TcpConnectionPtr& conn) {
        if (conn->connected()) {
            Header head;
            head.uid          = 1;
            head.request_type = static_cast<int>(RequestType::SIGN_UP);
            head.stamp        = index;
            player_info->set_nickname("player:" + to_string(index));
            ++index;
            auto [str, len] = formatMessage(head, player_info);
            conn->send(str, len);

        }
        else {
            cout << " bad connection with proxy" << endl;
        }
    });


    client.setMessageCallback([&](const TcpConnectionPtr& conn, Buffer* buffer, auto) {
        if (buffer->readableBytes()) {
            Header head(buffer->peek());
            if (head.request_type < 0) {
                cout << "sign up error with id " << head.stamp << endl;
            }
            else {
                cout << "success sign up with uid =  " << head.uid << endl;
            }
            buffer->retrieve(head.request_length());

            if(index == 100000) {
                cout << "finish" << endl;
                exit(0);
            }

            player_info->set_nickname("player:" + to_string(index));
            ++index;
            Header request;
            request.uid = 1;
            request.request_type = static_cast<int>(RequestType::SIGN_UP);
            request.stamp = index;
            auto [str, len] = formatMessage(request, player_info);
            conn->send(str, len);
        }
        });


    client.connect();
    loop.loop();
}
