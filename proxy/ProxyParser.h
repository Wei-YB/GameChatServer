#pragma once
#include "BasicParser.h"
#include "muduo/net/TcpConnection.h"

using muduo::net::Buffer;
using muduo::net::TcpConnectionPtr;
using muduo::net::TcpConnection;

enum class ClientState {
    LOGIN,
    LOGIN_REQUEST,
    SIGN_UP_REQUEST,
    LOGOUT_REQUEST,
    NOT_LOGIN,
};



class ProxyParser : public chatServer::BasicParser {
public:
    ProxyParser(const muduo::net::TcpConnectionPtr& connPtr);

    // just the forward policy
    void parseData(muduo::net::Buffer* buffer) override;

    void parseSignUp(muduo::net::Buffer* buffer);

    void parseLogin(Buffer* buffer);

    void parseNormalRequest(Buffer* buffer) const;

    void setLogin();

    ClientState getState() const;

private:

    std::weak_ptr<TcpConnection> conn_ptr_;
    ClientState client_state_;
};