#pragma once
#include <message.pb.h>
#include "Header.h"


namespace chatServer {

class Request {
public:
    Request() : header_(), message_(nullptr) {
    }

    void setRequestType(RequestType request) {
        header_.request_type = static_cast<int>(request);
        if (header_.requestType() == RequestType::LOGIN || header_.requestType() == RequestType::SIGN_UP) {
            header_.uid = -1;
        }
    }

    void setHeader(RequestType request, int uid) {
        header_.request_type = static_cast<int>(request);
        header_.uid = uid;
    }

    void setMessage(std::shared_ptr<Message> message) {
        message_ = message;
    }

    std::pair<const char*, int> format() {
        return formatMessage(header_, message_);
    }


private:
    static int nextStamp() {
        if (stamp_ == INT32_MAX) {

            stamp_ = 0;
            return stamp_;
        }
        else
            return ++stamp_;
    }

    inline static int stamp_ = 0;

    Header                   header_;
    std::shared_ptr<Message> message_;
};
}
