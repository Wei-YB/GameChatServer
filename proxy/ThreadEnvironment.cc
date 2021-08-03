#include "ThreadEnvironment.h"

#include "ProxyParser.h"

thread_local ThreadEnvironment* environment;

ThreadEnvironment::ThreadEnvironment(EventLoop* loop):
    data_server(loop, InetAddress("127.0.0.1", 12345),
                "Database Client"),
    chat_server(std::make_shared<TcpClient>(loop, InetAddress("127.0.0.1", 12346),
                                            "Chat Clinet")),
    current_stamp_(1),
    redis_client(loop, InetAddress("127.0.0.1", 6379)) {
    data_server.connect();
    data_server.setMessageCallback([this](const TcpConnectionPtr& conn, Buffer* buffer, Timestamp) {
        this->onDataServerMessage(buffer);
    });

    chat_server->setMessageCallback([this](const TcpConnectionPtr& conn, Buffer* buffer, Timestamp) {
        this->onChatServerMessage(buffer);
        });

    chat_server->setConnectionCallback([](const TcpConnectionPtr& conn) {
        if (conn->disconnected()) {
            environment->chat_server.reset();
        }
    });

    chat_server->connect();
    



    redis_client.connect();
    // environment->redis_client.command();
}

void ThreadEnvironment::onDataServerMessage(Buffer* buffer) {
    while (buffer->readableBytes()) {
        chatServer::Header head(buffer->peek());
        LOG_INFO << " receive from database: " << head.toString();
        auto uid = head.uid;
        // check the uid is in login_clients?
        auto stamp = head.stamp;
        if (sign_up_requests.count(stamp)) {
            // receive a response from database service
            auto [origin_stamp, client_conn] = sign_up_requests[stamp];
            sign_up_requests.erase(stamp);
            head.stamp = origin_stamp;
            if (auto conn = client_conn.lock()) {
                auto [str, len] = formatMessage(head, buffer->peek() + sizeof head);
                conn->send(str, len);
                LOG_INFO << "send " << len << " bytes to client";
            }
        }
        else
        {
            LOG_ERROR << " invalid response to client";
        }
        buffer->retrieve(head.request_length());
    }
}

void ThreadEnvironment::onChatServerMessage(Buffer* buffer) {
    while (buffer->readableBytes()) {
        Header header(buffer->peek());
        LOG_INFO << "receive from chat server: " << header.toString();
        auto uid = header.uid;
        auto stamp = header.stamp;
        if (header.request_type <= 0) { // this is a response
            if (login_requests.count(stamp)) {
                auto [origin_stamp, client_conn] = login_requests[stamp];
                login_requests.erase(stamp);
                header.stamp = origin_stamp;
                login_clients.insert({ header.uid, client_conn });
                if (auto conn = client_conn.lock()) {
                    auto [str, len] = chatServer::formatMessage(header, buffer->peek() + sizeof header);
                    conn->send(str, len);
                    LOG_INFO << "send " << len << " bytes to client";
                    auto parser = boost::any_cast<ProxyParser>(conn->getMutableContext());
                    parser->player_uid = header.uid;
                    parser->setLogin();
                }
            }
        }
        else if(login_clients.count(header.uid)){ // this is a request
            auto client_conn = login_clients[uid].lock();
            
            // boost::any_cast<ProxyParser>(client_conn->getMutableContext())->player_uid = header.uid;
            client_conn->send(buffer->peek(), header.request_length());
        }
        else {
            LOG_ERROR << "invalid response to client";
        }
        buffer->retrieve(header.request_length());
    }
}

int ThreadEnvironment::getStamp() {
    if (current_stamp_ == INT32_MAX) {
        current_stamp_ = 0;
    }
    return ++current_stamp_;
}
