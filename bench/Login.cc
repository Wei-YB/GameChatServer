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

Header login_request;
std::shared_ptr<PlayerInfo> player_info = nullptr;

void clientMessageCallback(const TcpConnectionPtr& conn, Buffer* buffer, Timestamp stamp);
void clientConnectionCallback(const TcpConnectionPtr& conn);


EventLoop* main_loop;
InetAddress proxy_addr("127.0.0.1", 23456);
int user_id = 1;


vector<std::shared_ptr<TcpClient>> normal_user;
int normal_user_count;
int normal_user_login_count = 0;
int normal_stage_message_count = 0;
int normal_stage_message_expect;

vector<std::shared_ptr<TcpClient>> test_user;
int test_stage;
Timestamp start_time;
Timestamp finish_time;
constexpr int test_user_count = 100;
int test_stage_message_count = 0;
int test_stage_message_expect;

shared_ptr<TcpClient> newTestClient() {
    auto client = make_shared<TcpClient>(main_loop, proxy_addr, "test client");
    client->setMessageCallback(clientMessageCallback);
    client->setConnectionCallback(clientConnectionCallback);
    client->connect();
    return client;
}

shared_ptr<TcpClient> newNormalClient() {
    auto client = make_shared<TcpClient>(main_loop, proxy_addr, "normal client");
    client->setMessageCallback(clientMessageCallback);
    client->setConnectionCallback(clientConnectionCallback);
    client->connect();
    return client;
}

void normalClientLoginFinish() {
    cout << "normal user login success: " << normal_user_login_count << endl;
    cout << "start login test with count = " << test_user_count << endl;
    test_stage = 1;
    for (int i = 0; i < test_user_count; ++i) {
        test_user.push_back(newTestClient());
    }
    start_time = Timestamp::now();
    
}

void testClientLoginFinish() {
    cout << "----all message received ----" << endl;
    const auto diff = finish_time.microSecondsSinceEpoch() - start_time.microSecondsSinceEpoch();
    const auto cost = diff / 1000.0;
    cout << "current user in lobby = " << normal_user_count << endl;
    cout << "login total cost = " << cost << " ms" << endl;
    cout << "login user count = " << normal_user_count << endl;
    cout << "login request speed = " << cost / normal_user_count << " ms" << endl;
    cout << "login tps = " << normal_user_count / (cost / 1000) << " transactions per second" << endl;
    exit(0);
}


void clientConnectionCallback(const TcpConnectionPtr& conn) {
    if (conn->connected()) {
        login_request.stamp = user_id;
        player_info->set_nickname("player:" + to_string(user_id));
        auto [str, len] = formatMessage(login_request, player_info);
        conn->send(str, len);
        // cout << "user: " << user_id << " try to login" << endl;
        ++user_id;
    }
    else {
        cout << "connection break with proxy" << endl;
        exit(0);
    }
}

int login_user_count = 0;

void clientMessageCallback(const TcpConnectionPtr& conn, Buffer* buffer, Timestamp stamp) {
    while (buffer->readableBytes()) {
        if (buffer->readableBytes() < sizeof(Header))
            break;
        Header head(buffer->peek());
        if (buffer->readableBytes() < head.request_length())
            break;
        buffer->retrieve(head.request_length());
        // valid request;
        if (head.request_type <= 0) {
            ++login_user_count;
            if (login_user_count == normal_user_count + test_user_count) {
                cout << "----all player login----" << endl;
                auto diff = stamp.microSecondsSinceEpoch() - start_time.microSecondsSinceEpoch();
                auto cost = diff / 1000.0;
                cout << "100 user login costs " << cost << " ms" << endl;
                cout << "login per second = " << 100 / cost * 1000 << " /s " << endl;
            }
        }
        else if (!test_stage) {
            ++normal_stage_message_count;
            // cout << " got message: " << normal_stage_message_count << endl;
            if (normal_stage_message_count == normal_stage_message_expect) {
                finish_time = stamp;
                main_loop->runAfter(1, testClientLoginFinish);
            }
        }
        else {
            ++test_stage_message_count;
            if (test_stage_message_count == test_stage_message_expect) {
                finish_time = stamp;
                main_loop->runAfter(1, testClientLoginFinish);
            }
        }
    }
}


void startLogin() {
    for (int i = 0; i < normal_user_count; ++i) {
        normal_user.push_back(newNormalClient());
    }
    start_time = Timestamp::now();
}


int main(int args, char* argv[]) {
    if (args != 2) {
        cout << "usage ./Login [normal client num]" << endl;
        return 0;
    }
    // ip = string(argv[2]);
    string useless;
    if (cin >> useless) {
        cout << "start!" << endl;
    }

    muduo::g_logLevel = Logger::ERROR;

    normal_user_count = stoi(argv[1]);
    EventLoop loop;
    main_loop   = &loop;
    player_info = make_shared<PlayerInfo>();

    player_info->set_password("pass");
    login_request.uid          = 1;
    login_request.request_type = static_cast<int>(RequestType::LOGIN);

    normal_stage_message_expect = (normal_user_count) * (normal_user_count + 1) / 2;
    test_stage = 0;

    test_stage_message_expect = (test_user_count + normal_user_count + 1) * test_user_count / 2;

    cout << "expect message = " << normal_stage_message_expect << endl;

    // loop.runEvery(0.1, [&loop]() {
    //     loginFunc(&loop);
    // });

    loop.runAfter(0.1, [&loop]() {
        cout << "start login stage" << endl;
        startLogin();
        });

    loop.loop();
}
