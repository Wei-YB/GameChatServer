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
}

void Lobby::logout(int uid) {
    LOG_INFO << "player: " << uid << " logout of server";
    local_players_.erase(uid);
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
        // TODO: Distributed chat service
        // else if(other_clients_.count(receiver)){
        //     auto target_server = other_clients_[receiver];
        //     // find the server where the client is in
        // }
    else {
        // this is an offline message
        LOG_DEBUG << "offline message for " << receiver;
        auto                         msg_str = msg->SerializeAsString();
        redis_client->command([](auto, redisReply* reply) {
            if (reply->type != REDIS_REPLY_INTEGER) {
                LOG_ERROR << "bad redis call with ret = " << reply->str;
            }
            return 0;
        }, "RPUSH %s:%d %s", "msg", receiver, msg_str.c_str());
        // TODO: database_client_.write(msg);
    }
}

void Lobby::newMessage(std::shared_ptr<Message>&& msg) {
    if (msg->type() == Message_MessageType_LOBBY_BROADCAST || msg->type() == Message_MessageType_SERVER_BROADCAST) {
        broadcastMessage(std::move(msg));
    }
    else {
        privateChatMessage(std::move(msg));
    }
}

void Lobby::handleMessage() {
    // for (auto client : active_clients_) {
    //     client->handleMessageQueue();
    // }
    // active_clients_.clear();
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
