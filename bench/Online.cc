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

int request_count = 0;
int success_login = 0;
int bad_login     = 0;

vector<shared_ptr<TcpClient>> client_vec;
int client_index = 0;
int client_count = 0;

int unsend_request_count;


Header login_request;
std::shared_ptr<PlayerInfo> player_info = nullptr;


int chatStamp = 10000000;

constexpr int chatMessageType = 406827112;


unordered_map<string, Timestamp> chatMessageSendTime;

vector<long long> time_cost_list;

int messageLimit = 10000;

string ip;

void sendChatMessage(int sender, int receiver) {
    if (sender >= client_index || receiver >= client_index) {
        cout << "bad sender or receiver" << endl;
        return;
    }
    // cout << ":send message from " << sender << " to " << receiver << endl;
    auto client = client_vec[sender];
    auto msg    = std::make_shared<Message>();
    msg->set_type(Message_MessageType_ONLINE_CHAT);
    msg->set_sender(sender + 10);
    msg->set_receiver(receiver + 10);

    msg->set_msg(to_string(++chatStamp));

    Header header;
    header.uid          = sender + 10;
    header.stamp        = chatMessageType;
    header.request_type = static_cast<int>(RequestType::CHAT);
    auto [str, len]     = formatMessage(header, msg);
    client->connection()->send(str, len);
    chatMessageSendTime[msg->msg()] = muduo::Timestamp::now();
    --messageLimit;
}

void resultCount() {
    auto success_count = time_cost_list.size();
    double time_cost   = 0;
    for (auto val : time_cost_list) {
        time_cost += val;
    }

    cout << "success message: " << success_count << endl;
    cout << "fail message: " << 10000 - success_count << endl;
    cout << "success avg delay: " << time_cost / success_count / 1000 << " ms" << endl;
    exit(0);
}

void startChatTest(EventLoop* loop) {

    if (messageLimit > 0) {
        auto sender   = rand() % client_index;
        auto receiver = rand() % client_index;
        sendChatMessage(sender, receiver);
        loop->runAfter(0.001, [loop]() {
            startChatTest(loop);
        });
    }
    else {
        cout << "last message send, wait for result" << endl;
        loop->runAfter(5, resultCount);
    }

}

shared_ptr<TcpClient> NewClient(EventLoop* loop, int index) {
    auto client = std::make_shared<TcpClient>(loop, InetAddress(ip, 23456), "Client");
    client->setConnectionCallback([index](const TcpConnectionPtr& conn) {
        if (conn->connected()) {
            login_request.stamp = index;
            player_info->set_nickname("player:" + to_string(index));
            auto [str, len] = formatMessage(login_request, player_info);
            conn->send(str, len);
        }
        else {
            cout << "connection break with proxy" << endl;
            cout << "total  request = " << client_count << endl;
            cout << "success login = " << success_login << endl;
            exit(0);
        }
    });

    client->setMessageCallback([index, loop](const TcpConnectionPtr& conn, Buffer* buffer, Timestamp receive_time) {
        while (buffer->readableBytes()) {
            Header header(buffer->peek());
            if (header.request_type == 0) {
                if (header.stamp == index) {
                    --request_count;
                    ++success_login;
                    // cout << "success login, client: " << index << " with uid = " << header.uid << endl;
                    if (success_login == client_count)
                        conn->getLoop()->runAfter(0.1, [loop]() {
                            startChatTest(loop);
                        });
                }
                buffer->retrieve(header.request_length());
            }
            else if (header.requestType() == RequestType::CHAT) {
                buffer->retrieve(sizeof header);
                Message msg;
                msg.ParseFromString(buffer->retrieveAsString(header.data_length));
                if (chatMessageSendTime.count(msg.msg())) {
                    // cout << "got message ";
                    auto cost = receive_time.microSecondsSinceEpoch() - chatMessageSendTime[msg.msg()].
                        microSecondsSinceEpoch();
                    time_cost_list.push_back(cost);
                }
            }
            else if (header.request_type > 0) {
                buffer->retrieve(header.request_length());
            }
            else {
                cout << "bad response from server" << header.toString() << endl;
                ++bad_login;
            }
        }
    });

    client->connect();
    return client;
}


void loginFunc(EventLoop* loop) {
    if (client_index == client_count) {
        cout << "finish login and and start chat" << endl;
        return;
    }
    if(unsend_request_count == 0) {
        cout << "send all request, just wait for response" << endl;
        return;
    }

    while (request_count < 1000 && unsend_request_count > 0) {
        ++request_count;
        --unsend_request_count;
        client_vec.push_back(NewClient(loop, ++client_index));
    }
    loop->runAfter(0.1, [loop]() {
        cout << "login func called with unsend request: " << unsend_request_count<< endl;
        loginFunc(loop);
    });
}


int main(int args, char* argv[]) {
    if (args != 3) {
        cout << "usage ./Online [client num] [ip]" << endl;
        return 0;
    }
    ip = string(argv[2]);
    string useless;
    if(cin >>useless) {
        cout << "start!" << endl;
    }

    muduo::g_logLevel = Logger::ERROR;

    client_count = stoi(argv[1]);

    unsend_request_count = client_count;
    EventLoop loop;

    player_info = make_shared<PlayerInfo>();

    player_info->set_password("pass");
    login_request.uid          = 1;
    login_request.request_type = static_cast<int>(RequestType::LOGIN);


    // loop.runEvery(0.1, [&loop]() {
    //     loginFunc(&loop);
    // });

    loop.runAfter(0.1, [&loop]() {
        loginFunc(&loop);
    });

    loop.loop();
}
