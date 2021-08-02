#include "ProxyConnection.h"

#include "Lobby.h"

namespace chatServer::chat {
hiredis::Hiredis* redis_client;
}


chatServer::chat::ProxyConnection::ProxyConnection(const TcpConnectionPtr& conn_ptr, Lobby& lobby):
    BasicParser(true), lobby_(lobby), ptr_(conn_ptr), login_(false) {
}

void chatServer::chat::ProxyConnection::parseData(Buffer* buffer) {
    LOG_DEBUG << "receive head: " << header_.toString();
    if (header_.requestType() == RequestType::LOGIN) {
        handleLogin(buffer);
    }
    else if (header_.requestType() == RequestType::CHAT) {
        handleChat(buffer);
    }
    else if (header_.requestType() == RequestType::LOGOUT) {
        assert(header_.data_length == 0);
        lobby_.logout(header_.uid);
    }
}



void chatServer::chat::ProxyConnection::disconnect() {
    // queue_.shutdown();
    //TODO: release the pointer with connection
}


void chatServer::chat::ProxyConnection::handleLogin(Buffer* buffer) {
    auto info = std::make_shared<PlayerInfo>();
    info->ParseFromString(buffer->retrieveAsString(header_.data_length));
    // redis async call to get information about the user, try check the login state
    redis_client->command([this, info, request_head = header_](Hiredis* redis, redisReply* reply) mutable {
        if (reply->type == REDIS_REPLY_INTEGER) {
            LOG_DEBUG << "bad username";
        }
        else if (reply->type != REDIS_REPLY_ARRAY) {
            LOG_ERROR << "redis login error";
        }
        if (reply->type != REDIS_REPLY_ARRAY) {
            request_head.setFail(-1);
            sendMessage(formatMessage(request_head));
            return;
        }

        // handle the reply
        const auto password = std::string(reply->element[2]->str, reply->element[2]->len);
        if (password != info->password()) {
            // bad password
            request_head.setFail(-2);
            sendMessage(formatMessage(request_head));
        }
        // valid login
        request_head.setOk();
        request_head.uid = std::stoi(reply->element[4]->str);
        info->set_uid(request_head.uid);
        info->set_area(std::stoi(reply->element[0]->str));
        info->set_signuptime(std::stoi(reply->element[1]->str));
        sendMessage(formatMessage(request_head, info));

        // change the state to login
        uid_ = request_head.uid;
        login_ = true;
        lobby_.login(info, ptr_.lock());
        LOG_INFO << "player: " << uid_ << " login the lobby";
    }, "EVAL %s 1 %s", login_cmd, info->nickname().c_str());
}

void chatServer::chat::ProxyConnection::handleChat(Buffer* buffer) {
    auto msg = std::make_shared<Message>();
    msg->ParseFromString(std::string(buffer->peek(), header_.data_length));
    LOG_DEBUG << "new message from: " << msg->sender() << " to " << msg->receiver();
    msg->set_stamp(now());
    lobby_.newMessage(std::move(msg));
    buffer->retrieve(header_.data_length);
}

// void chatServer::chat::ProxyConnection::handleMessageQueue() {
//     handleMessageQueue(0, queue_);
//     queue_.clear();
// }

void chatServer::chat::ProxyConnection::sendMessage(const std::pair<char*, size_t>& message) const {
    auto [str, len] = message;
    if (auto conn = ptr_.lock()) {
        conn->send(str, len);
    }
    else {
        LOG_ERROR << "proxy shutdown";
    }
}
