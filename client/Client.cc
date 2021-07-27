#include <sys/socket.h>
#include <string>
#include <iostream>
#include <sstream>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <unistd.h>
#include <co_routine.h>

#include "message.pb.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <Header.h>

auto logger = spdlog::basic_logger_mt("client", "logs/log.txt");

using namespace std;
using chatServer::PlayerInfo;
using namespace chatServer;

stCoRoutine_t* input_routine;
stCoRoutine_t* output_routine;

int g_conn;

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
    if (args.size() != 3) {
        cout << "bad args: LOGIN <username> <password>" << endl;
        return;
    }
    auto username = args[1];
    auto password = args[2];

    auto info = std::make_shared<PlayerInfo>();
    info->set_nickname(username);
    info->set_password(password);

    Header header{};
    header.request_type = static_cast<int>(RequestType::LOGIN);
    header.stamp = 0;
    header.uid = -1;
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

void output_func() {
    co_enable_hook_sys();
    cout << "input coroutine enable" << endl;
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
    auto       str = string(data, header.data_length);
    PlayerInfo info;
    info.ParseFromString(str);
    cout << "get response with " << info.DebugString() << endl;
}

void handleNotify(const Header& header, const char* data) {
    spdlog::info("got notify from server");
}

void input_func() {
    // spdlog::info("input coroutine enable");
    cout << "input coroutine enable" << endl;
    char buffer[1024];
    co_enable_hook_sys();
    while (true) {
        pollfd event;
        event.fd = g_conn;
        event.events = POLLIN | POLLHUP;
        co_poll(co_get_epoll_ct(), &event, 1, -1);

        auto ret = read(g_conn, buffer, sizeof buffer);
        spdlog::info("receive {0} bytes from server", ret);
        while (ret) {
            Header header(buffer);
            auto   next = buffer + sizeof header;
            spdlog::info("header is {}", header.toString());
            if (header.request_type < 1) {
                handleResponse(header, next);
            }
            else {
                handleNotify(header, next);
            }
            ret -= header.request_length();
        }
    }
}


int main() {
    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::debug);
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
