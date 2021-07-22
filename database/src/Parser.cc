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
}

Parser::Parser(const ConnPtr& conn): conn_(conn),
                                     buffer_{},
                                     data_length_(0),
                                     request_type_(RequestType::UNKNOWN),
                                     uid_(-1),
                                     state_(State::WAIT_LENGTH) {
    LOG_TRACE << "Request Parser ctor with conn_ ref cnt = " << conn_.use_count();
}

void Parser::parseInfo(std::shared_ptr<PlayerInfo> info) {
    redis_client->command([this, info](auto, redisReply* reply) {
        if (reply->type == REDIS_REPLY_NIL) {
            failResponse(-1);
        }
        else {
            assert(reply->type == REDIS_REPLY_ARRAY);
            assert(reply->elements == 3);
            info->set_nickname(reply->element[2]->str);
            info->set_signuptime(std::stoi(reply->element[0]->str));
            info->set_password(std::string(reply->element[1]->str, reply->element[1]->len));
        }
    }, "LRANGE %d:player 0 -1", uid_);
}

void Parser::successResponse(std::shared_ptr<PlayerInfo> info) {
    sendResponse(0, info);
}

void Parser::failResponse(int status) {
    sendResponse(status);
}

void Parser::sendResponse(int status) {
    LOG_INFO << "relpy with status = " << status;
    chatServer::fillHead({0, status, -1}, buffer_);
    conn_->send(buffer_, sizeof(int[3]));
}

void Parser::sendResponse(int status, std::shared_ptr<PlayerInfo> info) {
    LOG_INFO << "relpy with status = " << status;
    const auto info_str = info->SerializeAsString();
    chatServer::fillHead(info_str.length(), 0, uid_, buffer_);
    memcpy(buffer_ + sizeof(int[3]), info_str.c_str(), info_str.size());
    if (this->conn_.use_count() == 0) {
        LOG_ERROR << "bad connection";
    }
    this->conn_->send(buffer_, info_str.size() + sizeof(int[3]));
}

void Parser::parse(Buffer* buffer) {
    if (state_ == State::WAIT_LENGTH && buffer->readableBytes() >= sizeof(int)) {
        data_length_ = buffer->readInt32();
        LOG_TRACE << "new request, data len = " << data_length_;
        state_ = State::WAIT_TYPE;
    }
    if (state_ == State::WAIT_TYPE && buffer->readableBytes() >= sizeof(int)) {
        // fixed: when receive integer out of range, the enum val is undefined.
        const auto type = buffer->readInt32();
        if (type < 0 || type >= static_cast<int>(RequestType::MAX_REQUEST_TYPE)) {
            LOG_WARN << "receive bad request type from " << conn_->peerAddress().toIpPort();
            request_type_ = RequestType::UNKNOWN;
        }
        request_type_ = static_cast<RequestType>(type);
        state_ = State::WAIT_UID;
    }
    if (state_ == State::WAIT_UID && buffer->readableBytes() >= sizeof(int)) {
        uid_ = buffer->readInt32();
        state_ = State::WAIT_DATA;
        LOG_DEBUG << "wait for data and buffer remains with size = " << buffer->readableBytes();
    }
    if (state_ == State::WAIT_DATA && buffer->readableBytes() >= data_length_) {
        LOG_DEBUG << "recv request, head: len = " << data_length_ << ", type = "<< static_cast<int>(request_type_) << ", uid = "<< uid_;
        // receive a full request, according the type pick a right buffer type
        if (request_type_ == RequestType::LOGIN) {
            auto playerInfo = getPlayerInfo(buffer);
            LOG_DEBUG << "playerInfo: name = " << playerInfo->nickname() << ", stamp = " << playerInfo->stamp();
            parseLogin(playerInfo);
        }
        else if (request_type_ == RequestType::SIGN_UP) {
            parseSignUp(getPlayerInfo(buffer));
        }
        else if (request_type_ == RequestType::INFO) {
            parseInfo(getPlayerInfo(buffer));
        }else {
            buffer->retrieve(data_length_);
            failResponse(-1);
        }
        state_ = State::WAIT_LENGTH;
    }
}

void Parser::disConnect() {
    LOG_TRACE << "release the connection ptr in parser";
    conn_.reset();
}

std::shared_ptr<chatServer::PlayerInfo> Parser::getPlayerInfo(Buffer* buffer) const {
    auto playerInfo = std::make_shared<PlayerInfo>();
    playerInfo->ParseFromString(buffer->retrieveAsString(data_length_));
    return playerInfo;
}

void Parser::parseSignUp(std::shared_ptr<PlayerInfo> info) {
    info->set_signuptime(chatServer::now());
    redis_client->command([this, info](auto, redisReply* reply) {
                              assert(reply->type == REDIS_REPLY_INTEGER);
                              if (reply->integer) {
                                  successResponse(info);
                              }
                              else {
                                  failResponse(-1);
                              }
                          }, "EVAL %s %d %s %s %d", sign_up_cmd, 3,
                          info->nickname().c_str(), info->password().c_str(), info->signuptime());
}

void Parser::parseLogin(std::shared_ptr<PlayerInfo> info) {
    LOG_DEBUG << "new request is login";
    redis_client->command([this, info](auto, auto* reply) {
                              LOG_INFO << "get redis reply with type = " << reply->type;
                              if (reply->type == REDIS_REPLY_ERROR) {
                                  LOG_ERROR << "redis err: " << reply->str;
                              }
                              else if (reply->type == REDIS_REPLY_INTEGER && reply->integer == 0) {
                                  sendResponse(-1);
                              }
                              else if (reply->type == REDIS_REPLY_ARRAY) {
                                  LOG_DEBUG << "reply size is " << reply->elements;
                                  uid_ = std::stoi(reply->element[0]->str);
                                  LOG_DEBUG << "got uid = " << uid_;
                                  info->set_password(reply->element[1]->element[1]->str);
                                  info->set_signuptime(std::stoi(reply->element[1]->element[0]->str));
                                  this->sendResponse(0, info);
                              }
                              else {
                                  sendResponse(-2);
                              }
                          },
                          "EVAL %s %d %s", login_cmd, 1, info->nickname().c_str());
}
