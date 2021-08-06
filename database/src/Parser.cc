#include "Parser.h"

#include <hiredis/hiredis.h>

#include <spdlog/spdlog.h>

using namespace chatServer::database;


static const auto* login_cmd =
    "if(redis.call('EXISTS', KEYS[1]..':uid'))==0 "
    "then return 0 end; "
    "local uid =  redis.call('GET', KEYS[1]..':uid'); "
    "return {uid, redis.call('LRANGE', uid..':player', 0, -1)};";

static const auto* info_cmd =
    "local uid = redis.call('GET', KEYS[1]..':uid');"
    "if(uid == false) then return 0 end;"
    "local info = redis.call('LRANGE', uid..':player', 0, -1);"
    "local status = redis.call('GET', uid..':online');"
    "return {info, status};";

static const auto* sign_up_cmd =
    "if(redis.call('EXISTS', KEYS[1]..':uid') == 1) "
    "then return 0 end;"
    "redis.call('INCR', 'UID');"
    "local uid = redis.call('GET', 'UID');"
    "redis.call('SET', KEYS[1]..':uid', uid);"
    "redis.call('LPUSH', uid..':player', uid, KEYS[1], KEYS[2], KEYS[3], KEYS[4]);"
    "return uid;";

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
        spdlog::warn("receive bad head");
        // LOG_WARN << "receive bad head";
        parseBadHead(buffer);
    }
    spdlog::info("got request head: {0}", header_.toString());
    const auto request_type = static_cast<RequestType>(header_.request_type);
    switch (request_type) {
    case RequestType::SIGN_UP:
        parseSignUp(getPlayerInfo(buffer));
        break;
    case RequestType::INFO:
        parseInfo (getPlayerInfo(buffer));
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
    spdlog::debug("try to get info: {0}", info->nickname());
    redis_client->command([this, info, request_head = header_](auto, redisReply* reply) mutable {
        if(reply->type == REDIS_REPLY_INTEGER) {
            spdlog::error("username not exists!");
            request_head.setFail();
            response(formatMessage(request_head));
            return 0;
        }
        if(reply->type != REDIS_REPLY_ARRAY) {
            spdlog::error("bad redis response when get info, type is {0}", reply->type);
            request_head.setFail();
            response(formatMessage(request_head));
            return 0;
        }
        auto player_info = reply->element[0];
        if (reply->element[1]->type == REDIS_REPLY_NIL) {
            info->set_online(0);
        }else {
            ServerInfo server_info;
            server_info.ParseFromString(reply->element[1]->str);
            info->set_online(server_info.area());
        }
        auto info_reply = reply->element[0];
        info->set_uid(std::stoi(info_reply->element[4]->str));
        info->set_signuptime(std::stoi(info_reply->element[1]->str));

        header_.setOk();
        response(formatMessage(header_, info));

        return 0;
    }, "EVAL %s 1 %s", info_cmd, info->nickname().c_str());
}

void Parser::disConnect() {
    spdlog::trace("release the connection ptr in parser");
    // LOG_TRACE << "release the connection ptr in parser";
    conn_.reset();
}

void Parser::parseSignUp(std::shared_ptr<PlayerInfo> info) {
    info->set_signuptime(chatServer::now());
    redis_client->command([this, info, request_head = header_](auto, redisReply* reply) mutable {
                              // assert(reply->type == REDIS_REPLY_INTEGER);
                              if (reply->type == REDIS_REPLY_STRING) {
                                  request_head.setOk();
                                  info->set_uid(std::stoi(reply->str));
                                  response(formatMessage(request_head, info));
                                  spdlog::debug("username: {0} success sign up", info->nickname());
                                  // LOG_DEBUG << "username: " << info->nickname() << "success sign up";
                                  return 0;
                              }
                              if (reply->type == REDIS_REPLY_INTEGER)
                              {
                                  spdlog::debug("username: {0} already used", info->nickname());
                                  // LOG_DEBUG << "username: " << info->nickname() << " already used";
                              }else {
                                  spdlog::error("redis error");
                                  // LOG_ERROR <<"redis error";
                              }
                              request_head.setFail();
                              response(formatMessage(request_head));
                              return 0;
                          }, "EVAL %s %d %s %s %d %d", sign_up_cmd, 4,
                          info->nickname().c_str(), info->password().c_str(), info->signuptime(), info->area());
}

void Parser::parseLogin(std::shared_ptr<PlayerInfo> info) {
    spdlog::debug("new request is login");
    //LOG_DEBUG << "new request is login";
    redis_client->command([this, info, request_head = header_](auto, auto* reply) mutable {
        spdlog::info("get redis reply with type = {}", reply->type);
        // LOG_INFO << "get redis reply with type = " << reply->type;
        if (reply->type == REDIS_REPLY_ARRAY) {
            // LOG_DEBUG << "reply size is " << reply->elements;
            spdlog::debug("reply size is {}", reply->elements);
            request_head.uid = std::stoi(reply->element[0]->str);
            // LOG_DEBUG << "got uid = " << request_head.uid;
            spdlog::debug("got uid = ", request_head.uid);
            info->set_password(reply->element[1]->element[1]->str);
            info->set_signuptime(std::stoi(reply->element[1]->element[0]->str));
            header_.setOk();
            response(formatMessage(request_head, info));
            return 0;
        }
        if (reply->type == REDIS_REPLY_ERROR) {
            // LOG_ERROR << "redis err: " << reply->str;
            spdlog::error("redis err: {}", reply->str);
            request_head.setFail(-3);
        }
        else if (reply->type == REDIS_REPLY_INTEGER && reply->integer == 0) {
            request_head.setFail();
        }
        else {
            request_head.setFail(-2);
        }
        response(formatMessage(request_head));
        return 0;
                          },
                          "EVAL %s %d %s", login_cmd, 1, info->nickname().c_str());
}

void Parser::parseBadHead(Buffer* buffer) {
    buffer->retrieveAll();
    // buffer->retrieve(header_.data_length);
    header_.setFail();
    response(formatMessage(header_));
}

void Parser::response(const std::pair<char*, size_t>& message) const {
    auto [str, len] = message;
    conn_->send(str, len);
}
}
