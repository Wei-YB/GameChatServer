#include <co_routine.h>

#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/TcpClient.h>
#include <muduo/base/Logging.h>

#include <BasicParser.h>
#include <muduo/base/Timestamp.h>
using namespace muduo::net;
using muduo::Timestamp;
using chatServer::BasicParser;
using chatServer::RequestType;
using chatServer::Header;



enum class ClientState {
    LOGIN,
    LOGIN_REQUEST,
    SIGN_UP_REQUEST,
    LOGOUT_REQUEST,
    NOT_LOGIN,
};

// proxy is harder then I think
struct ThreadEnvironment {
    ThreadEnvironment(EventLoop* loop) :
        data_server(loop, InetAddress("127.0.0.1", 12345),
                    "Database Client"), current_stamp_(1) {
        data_server.connect();
        data_server.setMessageCallback([this](const TcpConnectionPtr& conn, Buffer* buffer, Timestamp) {
            while (buffer->readableBytes()) {
                Header head(buffer->peek());
                LOG_INFO << " receive from database: " << head.toString();
                auto   uid = head.uid;
                // check the uid is in login_clients?
                auto stamp = head.stamp;
                if (sign_up_requests.count(stamp)) {
                    // receive a response from database service
                    auto [origin_stamp, client_conn] = sign_up_requests[stamp];
                    sign_up_requests.erase(stamp);
                    head.stamp = origin_stamp;
                    if (auto conn = client_conn.lock()) {
                        auto [str, len] = chatServer::formatMessage(head, buffer->peek() + sizeof head);
                        conn->send(str, len);
                        LOG_INFO << "send " << len << " bytes to client";
                    }
                }
                else {
                    LOG_ERROR << " invalid response to client";
                }
                buffer->retrieve(head.request_length());
            }
        });
    }

    TcpClient data_server;
    // TcpClient chat_server;

    // std::unordered_map<int, std::weak_ptr<TcpConnection>>                 login_clients;
    // std::unordered_map<int, std::pair<int, std::weak_ptr<TcpConnection>>> login_requests;
    std::unordered_map<int, std::pair<int, std::weak_ptr<TcpConnection>>> sign_up_requests;

    int getStamp() {
        if (current_stamp_ == INT32_MAX) {
            current_stamp_ = 0;
        }
        return ++current_stamp_;
    }

private:
    int current_stamp_;
};
// thread_local 
thread_local ThreadEnvironment* environment;


class ProxyParser : public BasicParser {
public:
    ProxyParser(const TcpConnectionPtr& connPtr) : BasicParser(false),
                                                   conn_ptr_(connPtr),
                                                   client_state_(ClientState::NOT_LOGIN) {
    }

    // just the forward policy
    void parseData(Buffer* buffer) override {
        switch (header_.requestType()) {
        case RequestType::SIGN_UP:
            parseSignUp(buffer);
            break;
        case RequestType::LOGIN:
            parseLogin(buffer);
            break;
        default:
            buffer->retrieve(header_.data_length);
        }

    }

    void parseSignUp(Buffer* buffer) {
        if (client_state_ != ClientState::NOT_LOGIN) {
            buffer->retrieve(header_.request_length());
            header_.setFail(-1);
            auto [str, len] = chatServer::formatMessage(header_);
            // bug: need to check the lock state
            conn_ptr_.lock()->send(str, len);
        }
        buffer->retrieve(sizeof header_);
        auto stamp = environment->getStamp();
        environment->sign_up_requests.insert({stamp, {header_.stamp, conn_ptr_}});
        header_.stamp = stamp;
        // fixed: did not change the stamp in buffer
        // environment->data_server.connection()->send(buffer->peek(), header_.request_length());
        auto [str, len] = formatMessage(header_, buffer->peek());
        environment->data_server.connection()->send(str, len);
        buffer->retrieve(header_.data_length);
    }

    // TODO: parse login
    void parseLogin(Buffer* buffer) const {
        buffer->retrieve(header_.request_length());
    }

    void setLogin() {
        client_state_ = ClientState::LOGIN;
    }

    ClientState getState() const {
        return client_state_;
    }

private:

    std::weak_ptr<TcpConnection> conn_ptr_;
    ClientState client_state_;
};

int main() {
    EventLoop loop;
    TcpServer server(&loop, InetAddress(23456), "proxy");
    // each thread will alloc a client to database and chat server

    server.setThreadNum(8);
    server.setThreadInitCallback([](auto* loop) {
        environment = new ThreadEnvironment(loop);
    });

    server.setConnectionCallback([](const TcpConnectionPtr& connPtr) {
        connPtr->setContext(ProxyParser(connPtr));
    });

    server.setMessageCallback([](const TcpConnectionPtr& connptr, Buffer* buffer, auto) {
        auto parser = boost::any_cast<ProxyParser>(connptr->getMutableContext());
        parser->parse(buffer);
        });

    muduo::g_logLevel = muduo::Logger::DEBUG;
    
    server.start();
    loop.loop();
}
