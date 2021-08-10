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

int messageLimit;

string ip;

Timestamp start_time;

void Broadcast(int sender) {
    if (sender >= client_index) {
        cout << "bad sender" << endl;
        return;
    }
    // cout << ":send message from " << sender << " to " << receiver << endl;
    auto client = client_vec[sender];
    auto msg    = std::make_shared<Message>();
    msg->set_type(Message_MessageType_SERVER_BROADCAST);
    msg->set_sender(sender + 10);

    msg->set_msg(to_string(sender));

    Header header;
    header.uid          = sender + 10;
    header.stamp        = chatMessageType;
    header.request_type = static_cast<int>(RequestType::CHAT);
    auto [str, len]     = formatMessage(header, msg);
    client->connection()->send(str, len);
    // chatMessageSendTime[msg->msg()] = muduo::Timestamp::now();
}

void resultCount() {
    auto success_count = time_cost_list.size();
    double time_cost   = 0;
    for (auto val : time_cost_list) {
        time_cost += val;
    }
    // cout << "success message: " << success_count << endl;
    // cout << "fail message: " << messageLimit - success_count << endl;
    // cout << "success avg delay: " << time_cost / success_count / 1000 << " ms" << endl;

    cout << "total broadcast message: " << messageLimit << endl;
    cout << "total cost time: " << time_cost / 1000 << " ms" << endl;
    cout << "broadcast message tps = " << messageLimit * 1000/ (time_cost / 1000) << " message per second" << endl;
    cout << "broadcast message average delay = " << (time_cost / 1000) / messageLimit << " ms" << endl;
    exit(0);
}

// void startChatTest(EventLoop* loop) {
//
//     if (messageLimit > 0) {
//         auto sender   = rand() % client_index;
//         auto receiver = rand() % client_index;
//         sendChatMessage(sender, receiver);
//         loop->runAfter(0.001, [loop]() {
//             startChatTest(loop);
//         });
//     }
//     else {
//         cout << "last message send, wait for result" << endl;
//         loop->runAfter(5, resultCount);
//     }
//
// }

void startBroadcastTest(EventLoop* loop) {
    int send = 0;
    while (true) {
        for (int i = 0; i < client_count; ++i) {
            Broadcast(i);
            ++send;
            if (send == messageLimit) {
                cout << "all message send" << endl;
                start_time = Timestamp::now();
                return;
            }
        }

    }
}

vector<int> message_receive_count;

int finish_count = 0;

shared_ptr<TcpClient> NewClient(EventLoop* loop, int index) {
    auto client = std::make_shared<TcpClient>(loop, InetAddress("127.0.0.1", 23456), "Client");
    client->setConnectionCallback([index](const TcpConnectionPtr& conn) {
        if (conn->connected()) {
            login_request.stamp = index;
            login_request.uid   = index % 2 + 1;
            player_info->set_nickname("player:" + to_string(index));
            auto [str, len] = formatMessage(login_request, player_info);
            conn->send(str, len);
            cout << "user: " << index << " try to login" << endl;
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
            if (buffer->readableBytes() >= sizeof(Header))
            {
                Header header(buffer->peek());
                if (buffer->readableBytes() >= header.request_length()) {
                    if (header.request_type == 0) {
                        if (header.stamp == index) {
                            --request_count;
                            ++success_login;
                            // cout << "success login, client: " << index << " with uid = " << header.uid << endl;
                            if (success_login == client_count)
                                conn->getLoop()->runAfter(0.1, [loop]() {
                                startBroadcastTest(loop);
                                    });
                        }
                        buffer->retrieve(header.request_length());
                    }
                    else if (header.requestType() == RequestType::CHAT) {
                        buffer->retrieve(header.request_length());
                        ++message_receive_count[index - 1];
                        // cout << "user " << index << " got message " << message_receive_count[index] << endl;
                        if (message_receive_count[index - 1] == messageLimit) {
                            ++finish_count;
                            // cout << "user: " << index << " got all the message, total count is " << finish_count << endl;
                        }
                        if (finish_count == client_count) {
                            auto cost = receive_time.microSecondsSinceEpoch() - start_time.microSecondsSinceEpoch();
                            time_cost_list.push_back(cost);
                            resultCount();
                        }

                        // buffer->retrieve(sizeof header);
                        // Message msg;
                        // msg.ParseFromString(buffer->retrieveAsString(header.data_length));
                        // if (chatMessageSendTime.count(msg.msg())) {
                        //     // cout << "got message ";
                        //     auto cost = receive_time.microSecondsSinceEpoch() - chatMessageSendTime[msg.msg()].
                        //         microSecondsSinceEpoch();
                        //     time_cost_list.push_back(cost);
                        // }
                    }
                    else if (header.request_type > 0) {
                        buffer->retrieve(header.request_length());
                    }
                    else {
                        cout << "bad response from server" << header.toString() << endl;
                        ++bad_login;
                    }
                }else {
                    // cout << "current buffer size is " << buffer->readableBytes() << " but request " << header.request_length() << endl;
                    break;
                }
            }else {
                // cout << "buffer's size is not enough, just wait!" << endl;
                break;
            }
        }
    });

    client->connect();
    return client;
}


void loginFunc(EventLoop* loop) {
    // if (client_index > client_count) {
    //     cout << "finish login and and start broadcast" << endl;
    //     return;
    // }
    if (unsend_request_count == 0) {
        cout << "send all request, just wait for response" << endl;
        return;
    }

    while (request_count < 1000 && unsend_request_count > 0) {
        ++request_count;
        --unsend_request_count;
        client_vec.push_back(NewClient(loop, ++client_index));
    }
    loop->runAfter(0.1, [loop]() {
        cout << "login func called with unsend request: " << unsend_request_count << endl;
        loginFunc(loop);
    });
}


int main(int args, char* argv[]) {
    if (args != 3) {
        cout << "usage ./Online [client num] [broadcastMessage]" << endl;
        return 0;
    }
    // ip = string(argv[2]);

    messageLimit = stoi(argv[2]);
    string useless;
    if (cin >> useless) {
        cout << "start!" << endl;
    }

    muduo::g_logLevel = Logger::ERROR;

    client_count = stoi(argv[1]);

    message_receive_count = vector<int>(client_count, 0);

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
        cout << "start login stage" << endl;
        loginFunc(&loop);
    });

    loop.loop();
}
