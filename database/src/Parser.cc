#include "Parser.h"

#include <hiredis/hiredis.h>

#include <muduo/base/Logging.h>

using namespace chatServer::database;


static const auto* login_cmd =
    "if(redis.call('EXISTS', KEYS[1]..':uid'))==0 "
    "then return 0 end; "
    "local uid =  redis.call('GET', KEYS[1]..':uid'); "
    "return {uid, redis.call('LRANGE', uid..':player', 0, -1)};";

static const auto* sign_up_cmd =
    "if(redis.call('EXISTS', KEYS[1]..':uid') == 1) "
    "then return 0 end;"
    "redis.call('INCR', 'UID');"
    "local uid = redis.call('GET', 'UID');"
    "redis.call('SET', KEYS[1]..':uid', uid);"
    "redis.call('LPUSH', uid..':player', KEYS[1], KEYS[2], KEYS[3]);"
    "return 1;";

// no necessary to use lua script
// auto info_cmd = "";
// TODO: lua script for black list
// static const auto* add_black_list_cmd = "";
// static const auto* del_black_list_cmd = "";
// static const auto* get_black_list_cmd = "";

namespace chatServer::database {
thread_local hiredis::Hiredis* redis_client;

Parser::Parser(const ConnPtr& connection): BasicParser(true), conn_(connection) {
}

void Parser::parseData(Buffer* buffer) {
    if (header_.request_type <= 0 || header_.request_type >= static_cast<int>(RequestType::MAX_REQUEST_TYPE)) {
        LOG_WARN << "receive bad head";
        parseBadHead(buffer);
    }
    const auto request_type = static_cast<RequestType>(header_.request_type);
    switch (request_type) {
    case RequestType::SIGN_UP:
        parseSignUp(getPlayerInfo(buffer));
        break;
    case RequestType::LOGIN:
        parseLogin (getPlayerInfo(buffer));
        break;
    default:
        parseBadHead(buffer);
    }
}

std::shared_ptr<PlayerInfo> Parser::getPlayerInfo(Buffer* buffer) const {
    auto playerInfo = std::make_shared<PlayerInfo>();
    playerInfo->ParseFromString(buffer->retrieveAsString(header_.data_length));
    return playerInfo;
}

void Parser::parseInfo(std::shared_ptr<PlayerInfo> info) {
    redis_client->command([this, info, request_head = header_](auto, redisReply* reply) mutable {
        if (reply->type == REDIS_REPLY_NIL) {
            request_head.setFail();
            response(formatMessage(request_head));
        }
        else {
            assert(reply->type == REDIS_REPLY_ARRAY);
            assert(reply->elements == 3);
            info->set_nickname(reply->element[2]->str);
            info->set_signuptime(std::stoi(reply->element[0]->str));
            info->set_password(std::string(reply->element[1]->str, reply->element[1]->len));
            request_head.setOk();
            response(formatMessage(request_head, info));
        }
    }, "LRANGE %d:player 0 -1", header_.uid);
}

void Parser::disConnect() {
    LOG_TRACE << "release the connection ptr in parser";
    conn_.reset();
}

void Parser::parseSignUp(std::shared_ptr<PlayerInfo> info) {
    info->set_signuptime(chatServer::now());
    redis_client->command([this, info, request_head = header_](auto, redisReply* reply) mutable {
                              assert(reply->type == REDIS_REPLY_INTEGER);
                              if (reply->integer) {
                                  request_head.setOk();
                                  response(formatMessage(request_head, info));
                                  LOG_DEBUG << "username: " << info->nickname() << "success sign up";
                              }
                              else {
                                  LOG_DEBUG << "username: " << info->nickname() << " already used";
                                  request_head.setFail();
                                  response(formatMessage(request_head));
                              }
                          }, "EVAL %s %d %s %s %d", sign_up_cmd, 3,
                          info->nickname().c_str(), info->password().c_str(), info->signuptime());
}

void Parser::parseLogin(std::shared_ptr<PlayerInfo> info) {
    LOG_DEBUG << "new request is login";
    redis_client->command([this, info, request_head = header_](auto, auto* reply) mutable {
                              LOG_INFO << "get redis reply with type = " << reply->type;
                              if (reply->type == REDIS_REPLY_ARRAY) {
                                  LOG_DEBUG << "reply size is " << reply->elements;
                                  request_head.uid = std::stoi(reply->element[0]->str);
                                  LOG_DEBUG << "got uid = " << request_head.uid;
                                  info->set_password(reply->element[1]->element[1]->str);
                                  info->set_signuptime(std::stoi(reply->element[1]->element[0]->str));
                                  header_.setOk();
                                  response(formatMessage(request_head, info));
                                  return;
                              }
                              if (reply->type == REDIS_REPLY_ERROR) {
                                  LOG_ERROR << "redis err: " << reply->str;
                                  request_head.setFail(-3);
                              }
                              else if (reply->type == REDIS_REPLY_INTEGER && reply->integer == 0) {
                                  request_head.setFail();
                              }
                              else {
                                  request_head.setFail(-2);
                              }
                              response(formatMessage(request_head));
                          },
                          "EVAL %s %d %s", login_cmd, 1, info->nickname().c_str());
}

void Parser::parseBadHead(Buffer* buffer) {
    buffer->retrieve(header_.data_length);
    header_.setFail();
    response(formatMessage(header_));
}

void Parser::response(const std::pair<char*, size_t>& message) const {
    auto [str, len] = message;
    conn_->send(str, len);
}
}
