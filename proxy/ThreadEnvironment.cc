#include "ThreadEnvironment.h"

#include "ProxyParser.h"

thread_local ThreadEnvironment* environment;

ThreadEnvironment::ThreadEnvironment(EventLoop* loop):
    data_server(loop, InetAddress("127.0.0.1", 12345),
        "Database Client"),
    redis_client(loop, InetAddress("127.0.0.1", 6379)),
    current_stamp_(1) {
    data_server.connect();
    data_server.setMessageCallback([this](auto conn, auto buffer, auto) {
        this->onDataServerMessage(buffer);
    });

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
        if (login_clients.count(uid)) {
            // just forward
            auto player_conn = login_clients[uid];
            player_conn.lock()->send(buffer->peek(), head.request_length());
        }
        else if (sign_up_requests.count(stamp)) {
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
        else {
            LOG_ERROR << " invalid response to client";
        }
        buffer->retrieve(head.request_length());
    }
}

void ThreadEnvironment::onChatServerMessage(Buffer* buffer) {
    while(buffer->readableBytes() >= sizeof(Header)) {
        Header header(buffer->peek());
        auto request_length = header.request_length();
        if(buffer->readableBytes() < request_length) {
            LOG_INFO << "buffer size not enough, wait";
            return;
        }
        LOG_INFO << "receive from chat server: " << header.toString();
        auto uid = header.uid;
        auto stamp = header.stamp;
        if (login_clients.count(header.uid)) {
            // forward to a valid player
            auto player_conn = login_clients[uid];
            player_conn.lock()->send(buffer->peek(), header.request_length());
        }
        else if (header.request_type <= 0 && login_requests.count(stamp)) {
            auto [origin_stamp, area, client_conn] = login_requests[stamp];
            login_requests.erase(stamp);
            header.stamp = origin_stamp;
            login_clients.insert({ header.uid, client_conn });
            user_info.insert({ header.uid, area });
            if (auto conn = client_conn.lock()) {
                auto [str, len] = chatServer::formatMessage(header, buffer->peek() + sizeof header);
                conn->send(str, len);
                LOG_INFO << "send " << len << " bytes to client";
                auto parser = boost::any_cast<ProxyParser>(conn->getMutableContext());
                parser->player_uid = header.uid;
                parser->setLogin();
            }
        }
        else {
            LOG_ERROR << "invalid response to client";
            buffer->retrieve(header.request_length());
            continue;
        }
        buffer->retrieve(request_length);
    }


    // while (buffer->readableBytes()) {
    //     if (buffer->readableBytes() < sizeof(Header)) {
    //         LOG_ERROR << "bad buffer size";
    //         buffer->retrieveAll();
    //         continue;
    //     }
    //     Header header(buffer->peek());
    //     auto request_length = header.request_length();
    //     if(buffer->readableBytes() < request_length) {
    //         LOG_ERROR << "bad buffer size";
    //         buffer->retrieveAll();
    //         continue;
    //     }
    //
    //     LOG_INFO << "receive from chat server: " << header.toString();
    //     auto uid = header.uid;
    //     auto stamp = header.stamp;
    //     if(login_clients.count(header.uid)) {
    //         // forward to a valid player
    //         auto player_conn = login_clients[uid];
    //         player_conn.lock()->send(buffer->peek(), header.request_length());
    //     }
    //     else if (header.request_type <= 0 && login_requests.count(stamp)) {
    //         auto [origin_stamp, area, client_conn] = login_requests[stamp];
    //         login_requests.erase(stamp);
    //         header.stamp = origin_stamp;
    //         login_clients.insert({ header.uid, client_conn });
    //         user_info.insert({ header.uid, area });
    //         if (auto conn = client_conn.lock()) {
    //             auto [str, len] = chatServer::formatMessage(header, buffer->peek() + sizeof header);
    //             conn->send(str, len);
    //             LOG_INFO << "send " << len << " bytes to client";
    //             auto parser = boost::any_cast<ProxyParser>(conn->getMutableContext());
    //             parser->player_uid = header.uid;
    //             parser->setLogin();
    //         }
    //     }else {
    //         LOG_ERROR << "invalid response to client";
    //         buffer->retrieveAll();
    //         continue;
    //     }
    //     if (buffer->readableBytes() < request_length) {
    //         LOG_ERROR << "buffer size < request length";
    //         buffer->retrieveAll();
    //     }else {
    //         buffer->retrieve(request_length);
    //     }
    // }
}

int ThreadEnvironment::getStamp() {
    if (current_stamp_ == INT32_MAX) {
        current_stamp_ = 0;
    }
    return ++current_stamp_;
}
