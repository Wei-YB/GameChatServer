#include <sys/socket.h>
#include <string>
#include <iostream>
#include <sstream>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <unistd.h>
#include <co_routine.h>
#include <time.h>

#include "message.pb.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <Header.h>

auto logger = spdlog::basic_logger_mt("client", "logs/log.txt");

using namespace std;
using chatServer::PlayerInfo;
using chatServer::Message;
using namespace chatServer;

stCoRoutine_t* input_routine;
stCoRoutine_t* output_routine;

int g_conn;
int g_uid = -1;
int g_area = 0;

PlayerInfo user_info;

vector<string> splitBySpace(const std::string& str) {
    vector<string> result;
    stringstream   s(str);
    string         temp;
    while (s >> temp) {
        result.push_back(temp);
    }
    return result;
}

void Login(const vector<string>& args) {
    if (args.size() != 3 && args.size() != 4) {
        cout << "bad args: LOGIN <username> <password> [area 1 | 2]" << endl;
        return;
    }
    if(g_uid != -1) {
        cout << "already login with user: " << user_info.nickname() << endl;
    }

    auto username = args[1];
    auto password = args[2];

    auto info = std::make_shared<PlayerInfo>();
    info->set_nickname(username);
    info->set_password(password);
    if (args.size() == 4) {
        const auto area = stoi(args[3]);
        if (area > 2 || area < 1) {
            cout << "invalid area, please input 1 or 2" << endl;
            return;
        }

        info->set_area(area);
    }
    else {
        info->set_area(1);
    }


    Header header{};
    header.request_type = static_cast<int>(RequestType::LOGIN);
    header.stamp        = 102;
    header.uid          = info->area();
    g_area              = info->area();

    auto [str, len] = formatMessage(header, info);
    spdlog::info("send {0} byte to server, with head = {1}", len, header.toString());
    write(g_conn, str, len);
}

void SignUp(const vector<string>& args) {
    if (args.size() != 4) {
        cout << "bad args: LOGIN <username> <password> <area>" << endl;
        return;
    }
    auto username = args[1];
    auto password = args[2];
    int  area = 0;
    try {
        area = stoi(args[3]);
        if (area > 3 || area < 1) {
            cout << "bad area name, please select 1 or 2" << endl;
        }
    }
    catch (exception&) {
        cout << "bad area name, please input an integer" << endl;
    }
    auto info = std::make_shared<PlayerInfo>();
    info->set_area(area);
    info->set_nickname(username);
    info->set_password(password);

    Header header{};
    header.request_type = static_cast<int>(RequestType::SIGN_UP);
    header.stamp = 0;
    header.uid = -1;
    auto [str, len] = formatMessage(header, info);
    spdlog::info("send {0} byte to server, with head = {1}", len, header.toString());
    write(g_conn, str, len);
}

void Chat(const vector<string>& args) {
    if (g_uid < 0) {
        cout << "invalid state" << endl;
        return;
    }

    auto name = args[1];
    string uid;
    while (name.back() != ':') {
        uid.push_back(name.back());
        name.pop_back();
    }


    auto receiver = stoi(uid) + 9;

    auto msg = std::make_shared<Message>();
    msg->set_sender(g_uid);
    // msg->set_stamp(2);
    msg->set_receiver(receiver);
    msg->set_msg(args[2]);
    msg->set_type(Message_MessageType_ONLINE_CHAT);
    Header header;
    header.uid          = g_uid;
    header.stamp        = 2;
    header.request_type = static_cast<int>(RequestType::CHAT);

    auto [str, len] = formatMessage(header, msg);
    spdlog::info("send {0} byte to server, with head = {1}", len, header.toString());
    write(g_conn, str, len);
}

void Broadcast(const vector<string>& args) {
    if (g_uid == -1) {
        cout << "invalid command" << endl;
        return;
    }
    auto msg = std::make_shared<Message>();
    msg->set_sender(g_uid);
    msg->set_type(Message_MessageType_LOBBY_BROADCAST);
    msg->set_msg(args[1]);
    Header header;
    header.uid          = g_uid;
    header.stamp        = 2;
    header.request_type = static_cast<int>(RequestType::CHAT);

    auto [str, len] = formatMessage(header, msg);
    spdlog::info("send {0} byte to server, with head = {1}", len, header.toString());
    write(g_conn, str, len);
}

void BroadcastAll(const vector<string>& args) {
    if (g_uid == -1) {
        cout << "invalid command" << endl;
        return;
    }
    auto msg = std::make_shared<Message>();
    msg->set_sender(g_uid);
    msg->set_type(Message_MessageType_SERVER_BROADCAST);
    msg->set_msg(args[1]);
    Header header;
    header.uid          = g_uid;
    header.stamp        = 2;
    header.request_type = static_cast<int>(RequestType::CHAT);

    auto [str, len] = formatMessage(header, msg);
    spdlog::info("send {0} byte to server, with head = {1}", len, header.toString());
    write(g_conn, str, len);
}

void Logout(const vector<string>& args) {
    if(g_uid == -1) {
        cout << "invalid command, have not login" << endl;
        return;
    }
    Header header;
    header.data_length = 0;
    header.uid = g_uid;
    g_uid = -1;
    header.request_type = static_cast<int>(RequestType::LOGOUT);
    header.stamp = 0;
    auto [str, len] = formatMessage(header);
    spdlog::info("send {0} byte to server, with head = {1}", len, header.toString());
    write(g_conn, str, len);
}

vector<string>&  shrink(vector<string>& args, int size) {
    auto& str = args[size - 1];
    for(int i = size; i < args.size(); ++i) {
        str += " ";
        str += args[i];
    }
    while(args.size() > size) {
        args.pop_back();
    }
    return args;
}

void Info(const vector<string>& args) {
    if (g_conn < 0) {
        cout << "not login" << endl;
        return;
    }
    if (args.size() != 2) {
        cout << "INFO <name>" << endl;
        return;
    }
    auto player_info = std::make_shared<PlayerInfo>();
    player_info->set_nickname(args[1]);
    Header header;
    header.uid = g_uid;
    header.request_type = static_cast<int>(RequestType::INFO);
    header.stamp = 103;
    auto [str, len] = formatMessage(header, player_info);
    write(g_conn, str, len);
}

void AddBlackList(const vector<string>& args) {
    auto player_info = std::make_shared<PlayerInfo>();
    player_info->set_nickname(args[1]);
    auto name = args[1];
    string uid;
    while(name.back() != ':') {
        uid.push_back(name.back());
        name.pop_back();
    }
    
    player_info->set_uid(stoi(uid) + 9);
    Header header;
    header.uid = g_uid;
    header.request_type = static_cast<int>(RequestType::ADD_BLACK_LIST);
    header.stamp = 90;
    auto [str, len] = formatMessage(header, player_info);
    write(g_conn, str, len);
}

void DelBlackList(const vector<string>& args) {
    auto player_info = std::make_shared<PlayerInfo>();
    player_info->set_nickname(args[1]);
    auto name = args[1];
    string uid;
    while (name.back() != ':') {
        uid.push_back(name.back());
        name.pop_back();
    }

    player_info->set_uid(stoi(uid) + 9);
    Header header;
    header.uid = g_uid;
    header.request_type = static_cast<int>(RequestType::DEL_BLACK_LIST);
    header.stamp = 90;
    auto [str, len] = formatMessage(header, player_info);
    write(g_conn, str, len);
}

void GetPlayerList(const vector<string>& args) {
    Header header;
    header.uid = g_uid;
    header.request_type = static_cast<int>(RequestType::PLAYER_LIST);
    header.stamp = 203;
    auto [str, len] = formatMessage(header);
    write(g_conn, str, len);
}

void output_func() {
    co_enable_hook_sys();
    spdlog::info("input coroutine enable");
    cout << "please input your command..." << endl;
    // block by the user input
    char buffer[1024];
    while (true) {
        pollfd event;
        event.fd = STDIN_FILENO;
        event.events = POLLIN | POLLHUP;
        co_poll(co_get_epoll_ct(), &event, 1, -1);

        auto ret = read(STDIN_FILENO, buffer, sizeof buffer);
        spdlog::info("read {0} byte from keyboard", ret);
        string message(buffer, ret);
        auto   args = splitBySpace(message);
        if (args[0] == "LOGIN") {
            Login(args);
        }
        else if (args[0] == "SIGNUP") {
            SignUp(args);
        }
        else if(args[0] =="CHAT") {
            Chat(shrink(args, 3));
        }else if(args[0] == "BROADCAST") {
            Broadcast(shrink(args, 2));
        }else if(args[0] == "BROADCASTALL") {
            BroadcastAll(shrink(args, 2));
        }else if(args[0] == "LOGOUT") {
            Logout(args);
        }else if(args[0] == "INFO") {
            Info(args);
        }else if(args[0] == "ADD_BLACK_LIST") {
            AddBlackList(args);
        }else if(args[0] == "DEL_BLACK_LIST") {
            DelBlackList(args);
        }else if(args[0] == "PLAYER_LIST") {
            GetPlayerList(args);
        }
        else {
            cout << "unknown command, please retry" << endl;
        }
    }
}


void handleResponse(const Header& header, const char* data) {
    spdlog::info("got response from server");
    if(header.request_type < 0) {
        cout << "got bad response from server" << endl;
        return;
    }
    auto str = string(data, header.data_length);
    if (header.stamp > 100 && header.stamp < 200) {
        PlayerInfo info;
        info.ParseFromString(str);
        if (header.stamp == 102) {
            // this is login
            user_info = info;
            g_uid     = info.uid();
            cout << user_info.nickname() << " login success into area: " << g_area << endl;
        }
        // cout << "get response with " << info.DebugString() << endl;

        else if(header.stamp == 103) {
            cout << "Nickname: " << info.nickname() << endl;
            time_t time = info.signuptime();
            auto time_info = ctime(&time);
            cout << "Signup Time: " << time_info;
            if(info.online()) {
                cout << "Online: True" << endl;
                cout << "Area: " << info.online() << endl;
            }else {
                cout << "Online: False" << endl;
            }
        }
    }
    else if (header.stamp == 203) {
        PlayerList players;
        players.ParseFromString(str);
        cout << "current area online player: " << endl;
        for (int i = 0; i < players.players_size(); ++i) {
            auto& player = players.players(i);
            cout << "nickname: " << player.nickname() << endl;
        }
    }
}

void handleNotify(const Header& header, const char* data) {
    spdlog::info("got notify from server");
    if (header.requestType() == RequestType::CHAT) {
        std::string str(data, header.data_length);
        Message message;
        message.ParseFromString(str);

        if (message.type() == Message_MessageType_ONLINE) {
            if (message.sender() == g_uid) {
                return;
            }
            cout << "player:" << message.sender() - 9 << " is online" << endl;
            return;
        }
        else if (message.type() == Message_MessageType_LOBBY_BROADCAST) {
            if (message.sender() == g_uid) {
                return;
            }
        }
        else if (message.type() == Message_MessageType_SERVER_BROADCAST) {
            if (message.sender() == g_uid) {
                return;
            }
        }

        time_t tm = message.stamp();
        cout << ctime(&tm);
        cout << "new message from " << "player:" << message.sender() - 9 << endl;
        cout << message.msg() << endl;
    }
}

void input_func() {
    // spdlog::info("input coroutine enable");
    // cout << "input coroutine enable" << endl;
    char buffer[1024];
    co_enable_hook_sys();
    while (true) {
        pollfd event;
        event.fd = g_conn;
        event.events = POLLIN | POLLHUP;
        co_poll(co_get_epoll_ct(), &event, 1, -1);

        auto ret = read(g_conn, buffer, sizeof buffer);
        if (ret == 0) {
            cout << "connection break with server, abort..";
            exit(0);
        }
        spdlog::info("receive {0} bytes from server", ret);
        auto cur = buffer;
        while (ret > 0) {
            Header header(cur);
            auto   next = cur + sizeof header;
            spdlog::info("header is {}", header.toString());
            if (header.request_type < 1) {
                handleResponse(header, next);
            }
            else {
                handleNotify(header, next);
            }
            cur += header.request_length();
            ret -= header.request_length();
        }
    }
}


int main() {
    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::debug);
    spdlog::flush_on(spdlog::level::info);
    g_conn = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(23456);
    server.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(g_conn, reinterpret_cast<sockaddr*>(&server), sizeof server) < 0) {
        spdlog::error("can't connect to server: {}", strerror(errno));
        exit(0);
    }

    co_create(&input_routine, nullptr, reinterpret_cast<void*(*)(void*)>(input_func), nullptr);
    co_create(&output_routine, nullptr, reinterpret_cast<void*(*)(void*)>(output_func), nullptr);

    co_resume(input_routine);
    co_resume(output_routine);

    co_eventloop(co_get_epoll_ct(), nullptr, nullptr);


}
