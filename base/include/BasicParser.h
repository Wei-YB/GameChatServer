#pragma once

#include <functional>
#include "Header.h"

namespace muduo {namespace net {
class Buffer;
}}

namespace chatServer {
using muduo::net::Buffer;

class BasicParser {
public:
    BasicParser(bool retrieveHead) : retrieve_head_(retrieveHead), state_(State::WAIT_HEAD), header_() {
    }

    void parse(Buffer* buffer);

    void parseHead(Buffer* buffer);

    virtual void parseData(Buffer* buffer);

    virtual ~BasicParser() = default;

protected:
    bool ableToReadHead(Buffer* buffer);
    bool ableToReadData(Buffer* buffer) const;

    enum class State {
        WAIT_HEAD,
        WAIT_DATA,
    };

    bool   retrieve_head_;
    State  state_;
    Header header_;
};
}
