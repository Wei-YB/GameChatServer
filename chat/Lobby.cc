#include "Lobby.h"
#include <muduo/base/Logging.h>
// #include "Client.h"
using namespace chatServer::chat;

const auto* login_cache = "redis.call('SADD', 'area:1', KEYS[1]); redis.call('PUBLISH', 'area:1:channel', KEYS[1]);";


void Lobby::login(std::shared_ptr<PlayerInfo> player_info, const TcpConnectionPtr& conn) {
    auto       player = std::make_shared<Player>(player_info, conn);
    auto       area = player->info().area();
    const auto uid = player->uid();
    //redis_client->command([](auto, auto) {
    //     }, "EVAL %s 1 %d", login_cache, player->uid());
    local_players_.insert({player->uid(), std::move(player)});

    // online information
    redis_client->command([](auto, auto) { return 0; },
                          "SET %d:online %s", uid, current_server_str.c_str());
    // offline message
    redis_client->command([this, player_uid = uid](hiredis::Hiredis* client, redisReply* reply) {
                              auto user = local_players_[player_uid];
                              if (reply->type == REDIS_REPLY_ARRAY) {
                                  LOG_DEBUG << "got " << reply->elements << " offline messages";
                                  // there is some offline message
                                  for (int i = 0; i < reply->elements; ++i) {
                                      auto offline_message = std::make_shared<Message>();
                                      offline_message->ParseFromString(reply->element[i]->str);
                                      user->newMessage(std::move(offline_message));
                                      active_players_.insert(user);
                                  }
                                  client->command([](auto, auto) {
                                      return 0;
                                  }, "DEL %s:%d", "msg", player_uid);
                              }
                              else {
                                  LOG_ERROR << "bad redis call to get offline message";
                              }
                              return 0;
                          },
                          "LRANGE %s:%d 0 -1", "msg", uid);
    auto online_notify = std::make_shared<Message>();
    online_notify->set_msg("online");
    online_notify->set_receiver(0);
    online_notify->set_sender(uid);
    online_notify->set_type(Message_MessageType_ONLINE);
    broadcastMessage(std::move(online_notify));
}

void Lobby::logout(int uid) {
    LOG_INFO << "player: " << uid << " logout of server";
    redis_client->command([](auto, auto) { return 0; }, "DEL %d:online", uid);
    local_players_.erase(uid);

    auto offline_notify = std::make_shared<Message>();
    offline_notify->set_sender(uid);
    offline_notify->set_type(Message_MessageType_OFFLINE);
    offline_notify->set_msg("offline");

    broadcastMessage(std::move(offline_notify));
    // local_clients_.erase(uid);
}

void Lobby::broadcastMessage(std::shared_ptr<Message>&& msg) {
    broadcast_message_list_.push_back(msg);
    // TODO: notify other server;
}

void Lobby::privateChatMessage(std::shared_ptr<Message>&& msg) {
    auto receiver = msg->receiver();
    auto player = local_players_.find(receiver);
    if (player != local_players_.end()) {
        auto client = player->second;
        client->newMessage(std::move(msg));
        active_players_.insert(client);
    }
    else { // the receiver may on other server or offline
        LOG_DEBUG << "the receiver is not on current server";

        redis_client->command([this, msg = std::move(msg)](auto, redisReply* reply) mutable {
            if (reply->type == REDIS_REPLY_NIL) {
                offlineMessage(std::move(msg));
            }
            else if (reply->type == REDIS_REPLY_STRING) {
                std::string info(reply->str);
                forwardMessage(msg, info);
            }else {
                LOG_ERROR << "bad reply from redis";
            }
            return 0;
            }, "GET %d:online", receiver);
    }
}

void Lobby::forwardMessage(std::shared_ptr<Message> msg, std::string server) {
    if (other_servers_.count(server)) {
        LOG_DEBUG << "forward message "<< msg->DebugString() << " to dest";
        auto   client = other_servers_[server];
        Header header;
        header.request_type = static_cast<int>(RequestType::CHAT);
        header.uid = msg->sender();
        header.stamp = 0;
        auto [str, len] = formatMessage(header, msg);
        client->connection()->send(str, len);
    }else {
        LOG_WARN << "invalid server id";
    }
}

void Lobby::offlineMessage(std::shared_ptr<Message>&& msg) {
    auto receiver = msg->receiver();
    LOG_DEBUG << "offline message for " << receiver;
    auto                         msg_str = msg->SerializeAsString();
    redis_client->command([](auto, redisReply* reply) {
        if (reply->type != REDIS_REPLY_INTEGER) {
            LOG_ERROR << "bad redis call with ret = " << reply->str;
        }
        return 0;
    }, "RPUSH %s:%d %s", "msg", receiver, msg_str.c_str());
}

void Lobby::handleOfflineMessage(hiredis::Hiredis* client, redisReply* reply) {


}


void Lobby::newMessage(std::shared_ptr<Message>&& msg) {
    if (msg->type() == Message_MessageType_LOBBY_BROADCAST || msg->type() == Message_MessageType_SERVER_BROADCAST) {
        broadcastMessage(std::move(msg));
    }
    if (msg->type() == Message_MessageType_SERVER_BROADCAST && local_players_.count(msg->sender())) {
        for (auto& [serv, client] : other_servers_) {
            forwardMessage(msg, serv);
        }
    }
    else {
        privateChatMessage(std::move(msg));
    }
}

void Lobby::handleMessage() {
    for (auto player : active_players_) {
        player->handleMessageQueue();
    }
    active_players_.clear();
    if (!broadcast_message_list_.empty()) {
        for (auto [uid, player] : local_players_) {
            player->sendBroadcastMessage(broadcast_message_list_);
        }
        broadcast_message_list_.clear();
    }
}

void Lobby::newServerInfo(muduo::net::EventLoop* loop, const std::string& info) {
    ServerInfo server_info;
    server_info.ParseFromString(info);
    // they are same server
    if (server_info.ip() == current_server.ip() && server_info.port() == current_server.port())
        return;
    // already have a connection
    if (other_servers_.count(info))
        return;
    auto client = std::make_shared<TcpClient>
        (loop, muduo::net::InetAddress(server_info.ip(), server_info.port()), "other server");
    
    client->connect();
    LOG_DEBUG << "connect to server " << server_info.DebugString();
    other_servers_[info] = client;
}

void Lobby::start(muduo::net::EventLoop* loop) {
    // 1. get other server info from public set
    redis_client->command([this, loop](auto, redisReply* reply) {
        if (reply->type == REDIS_REPLY_NIL) {
            return 0;
        }
        if (reply->type == REDIS_REPLY_ARRAY) {
            // there is lots of server in the list
            for (int i = 0; i < reply->elements; ++i) {
                std::string info(reply->element[i]->str);
                newServerInfo(loop, info);
            }
        }
        return 0;
    }, "SMEMBERS server:info");
    // 2. publish current server info to the public channel

    redis_client->command([](auto, auto) {
        return 0;
    }, "SADD server:info %s", current_server_str.c_str());

    redis_client->command([](auto, auto) {
        return 0;
    }, "PUBLISH service_notify %s", current_server_str.c_str());
    // 3. listen to the channel and wait for other server online

    hiredis::Hiredis* subscribe_client =
        new hiredis::Hiredis(loop, muduo::net::InetAddress("127.0.0.1", 6379));

    subscribe_client->connect();

    loop->runAfter(1, [=]() {
        subscribe_client->command([this, loop](auto, redisReply* reply) {
            if (reply->elements == 3 && reply->element[2]->type == REDIS_REPLY_STRING) {
                LOG_INFO << "got new server info from channel service_notify";
                std::string info(reply->element[2]->str, reply->element[2]->len);
                newServerInfo(loop, info);
                LOG_DEBUG << "finish notify message handle";
            }
            else if (reply->elements != 3)
                LOG_ERROR << "bad redis reply";
            else {
                LOG_INFO << "got useless info from channel service_notify";
            }
            return 1;
        }, "SUBSCRIBE service_notify");
    });
}
