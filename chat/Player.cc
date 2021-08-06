#include "Player.h"

#include "muduo/base/Logging.h"

using namespace chatServer::chat;

Player::Player(const std::shared_ptr<PlayerInfo>& player_info, const TcpConnectionPtr& proxy):
    info_(*player_info), user_header_(), proxy_conn_weak_(proxy->weak_from_this()) {
    user_header_.uid = info_.uid();
}

void Player::resetProxy(const TcpConnectionPtr& proxy_conn) {
    proxy_conn_weak_ = proxy_conn;
}

bool Player::newMessage(std::shared_ptr<Message>&& msg) {
    if (isInBlackList(msg)) {
        return false;
    }
    message_list_.push_back(std::move(msg));
    return true;
}

void Player::handleMessageQueue() {
    // TODO: check the proxy connection
    lockProxyConn();
    if (message_list_.size() < 100) {
        // list contains less then 100 tips message, just send
        for (auto& msg : message_list_)
            sendMessageByShared(msg, RequestType::CHAT);
        message_list_.clear();
    }
    else {
        // send top 100 message, wait for the next loop to send
        for (int i = 0; i < 100; ++i) {
            sendMessageByShared(message_list_.front(), RequestType::CHAT);
            message_list_.pop_front();
        }
    }
    unlockProxyConn();
}

void Player::sendBroadcastMessage(const MessageList& msg_list) {
    lockProxyConn();
    for (auto msg : msg_list) {
        if (!isInBlackList(msg)) {
            sendMessageByShared(msg, RequestType::CHAT);
        }
    }
    unlockProxyConn();
}

void Player::addBlackList(int uid, const std::string& name) {
    black_list_.insert({uid, name});
}

void Player::delBlackList(int uid) {
    black_list_.erase(uid);
}

void Player::initBlackList(redisReply* reply) {
    for (int i = 0; i < reply->elements / 2; ++i) {
        auto black_player = std::stoi(reply->element[i * 2]->str);
        std::string name  = reply->element[i * 2 + 1]->str;
        black_list_.insert({black_player, name});
    }
}

void Player::sendMessageByWeak(const std::shared_ptr<Message>& msg, RequestType type) {
    lockProxyConn();
    sendMessageByShared(msg, type);
    unlockProxyConn();
}

bool Player::lockProxyConn() {
    auto conn = proxy_conn_weak_.lock();
    if (conn) {
        proxy_conn_share_.swap(conn);
        return true;
    }
    return false;
}

void Player::unlockProxyConn() {
    proxy_conn_share_.reset();
}

bool Player::isInBlackList(const std::shared_ptr<Message>& msg) {
    if (black_list_.count(msg->sender())) {
        LOG_DEBUG << "get message from block player: " <<
            "[" << msg->sender() << ", " << black_list_[msg->sender()] << "]";
        return true;
    }
    return false;
}
