#include "ProxyParser.h"
#include "ThreadEnvironment.h"

ProxyParser::ProxyParser(const muduo::net::TcpConnectionPtr& connPtr): BasicParser(false),
                                                                       conn_ptr_(connPtr),
                                                                       client_state_(ClientState::NOT_LOGIN) {
}

void ProxyParser::parseData(muduo::net::Buffer* buffer) {
    LOG_DEBUG << "try to parse the data with buffer size is " << buffer->readableBytes();
    switch (header_.requestType()) {
    case chatServer::RequestType::SIGN_UP:
        parseSignUp(buffer);
        break;
    case chatServer::RequestType::LOGIN:
        parseLogin(buffer);
        break;
    case chatServer::RequestType::INFO:
        parseInfo(buffer);
        break;
    case chatServer::RequestType::CHAT:
    case chatServer::RequestType::ADD_BLACK_LIST:
    case chatServer::RequestType::DEL_BLACK_LIST:
    case chatServer::RequestType::GET_BLACK_LIST:
    case chatServer::RequestType::PLAYER_LIST:
    case chatServer::RequestType::LOGOUT:
        parseNormalRequest(buffer);
        break;
    default:
        buffer->retrieve(header_.request_length());
    }

}

void ProxyParser::parseInfo(Buffer* buffer) const {
    environment->data_server.connection()->send(buffer->peek(), header_.request_length());
    buffer->retrieve(header_.request_length());
}



void ProxyParser::parseSignUp(muduo::net::Buffer* buffer) {
    if (client_state_ != ClientState::NOT_LOGIN) {
        LOG_DEBUG << "sign up called when not login";
        buffer->retrieve(header_.request_length());
        header_.setFail(-1);
        auto [str, len] = chatServer::formatMessage(header_);
        // bug: need to check the lock state
        conn_ptr_.lock()->send(str, len);
        return;
    }
    buffer->retrieve(sizeof header_);
    auto stamp = environment->getStamp();
    environment->sign_up_requests.insert({stamp, {header_.stamp, conn_ptr_}});
    header_.stamp = stamp;
    // fixed: did not change the stamp in buffer
    // environment->data_server.connection()->send(buffer->peek(), header_.request_length());
    auto [str, len] = formatMessage(header_, buffer->peek());
    environment->data_server.connection()->send(str, len);
    buffer->retrieve(header_.data_length);
}

void ProxyParser::parseLogin(Buffer* buffer) {
    if (client_state_ != ClientState::NOT_LOGIN || header_.uid > 2 || header_.uid < 0) {
        buffer->retrieve(header_.request_length());
        header_.setFail(-1);
        auto [str, len] = chatServer::formatMessage(header_);
        // bug: need to check the lock state
        conn_ptr_.lock()->send(str, len);
        return;
    }
    LOG_DEBUG << "valid request, handle login request";
    buffer->retrieve(sizeof header_);
    auto stamp = environment->getStamp();
    environment->login_requests.insert({stamp, {header_.stamp, header_.uid, conn_ptr_}});
    header_.stamp = stamp;
    // fixed: did not change the stamp in buffer
    // environment->data_server.connection()->send(buffer->peek(), header_.request_length());
    auto [str, len] = formatMessage(header_, buffer->peek());
    if (header_.uid == 1)
        environment->chat_server_area_1->connection()->send(str, len);
    else if (header_.uid == 2)
        environment->chat_server_area_2->connection()->send(str, len);
    LOG_DEBUG << "send " << len << " byte to chat server";
    buffer->retrieve(header_.data_length);
}

void ProxyParser::parseNormalRequest(Buffer* buffer) const {
    const auto area = environment->user_info[header_.uid];

    std::shared_ptr<TcpClient> server;
    if (area == 1) {
        server = environment->chat_server_area_1;
    }
    else if (area == 2) {
        server = environment->chat_server_area_2;
    }
    else {
        // TODO: bad request;
    }
    server->connection()->send(buffer->peek(), header_.request_length());
    buffer->retrieve(header_.request_length());
}

void ProxyParser::setLogin() {
    client_state_ = ClientState::LOGIN;
}

ClientState ProxyParser::getState() const {
    return client_state_;
}
