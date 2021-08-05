#pragma once
#include <list>
#include <muduo/net/TcpClient.h>

#include "Header.h"
#include "message.pb.h"

extern chatServer::ServerInfo current_server;
extern std::string current_server_str;
namespace chatServer::chat {

using muduo::net::TcpClient;
using muduo::net::TcpConnection;
using muduo::net::TcpConnectionPtr;


class Player {
public:
    using MessagePtr = std::shared_ptr<Message>;
    using MessageList = std::list<MessagePtr>;

    Player(const std::shared_ptr<PlayerInfo>& player_info, const TcpConnectionPtr& proxy);

    int uid() const {
        return info_.uid();
    }

    const PlayerInfo& info() const { return info_; }

    void resetProxy(const TcpConnectionPtr& proxy_conn);

    bool newMessage(std::shared_ptr<Message>&& msg);

    void handleMessageQueue();

    void sendBroadcastMessage(const MessageList& msg_list);

    void addBlackList(int uid);

    void delBlackList(int uid);

    void initBlackList();

private:
    void sendMessageByWeak(const std::shared_ptr<Message>& msg, RequestType type);

    bool lockProxyConn();

    void unlockProxyConn();

    void sendMessageByShared(const std::shared_ptr<Message>& msg, RequestType type) {
        if (isInBlackList(msg))
            return;
        user_header_.request_type = static_cast<int>(type);
        auto [str, len] = formatMessage(user_header_, msg);
        proxy_conn_share_->send(str, len);
    }

    bool isInBlackList(const std::shared_ptr<Message>& msg);

    PlayerInfo info_;
    MessageList message_list_;
    Header user_header_;
    std::unordered_map<int, std::string> black_list_;
    std::weak_ptr<TcpConnection> proxy_conn_weak_;
    TcpConnectionPtr proxy_conn_share_ = nullptr;
};
}
