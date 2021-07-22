#include "BasicParser.h"

#include <muduo/net/Buffer.h>
#include <muduo/base/Logging.h>
using namespace chatServer;

void BasicParser::parse(Buffer* buffer) {
    while (true) {
        if (state_ == State::WAIT_HEAD) {
            if (ableToReadHead(buffer)) {
                parseHead(buffer);
                state_ = State::WAIT_DATA;
            }
            else
                break;
        }
        if (state_ == State::WAIT_DATA) {
            if (ableToReadData(buffer)) {
                parseData(buffer);
                state_ = State::WAIT_HEAD;
            }
            else
                break;
        }
    }
}

void BasicParser::parseHead(Buffer* buffer) {
    memcpy(&header_, buffer->peek(), sizeof header_);
    LOG_DEBUG << "got full head: " << header_.toString();
    if(retrieve_head_)
        buffer->retrieve(sizeof header_);
}

void BasicParser::parseData(Buffer* buffer) {
    buffer->retrieve(header_.data_length + (retrieve_head_ ? 0 : sizeof header_));
}

bool BasicParser::ableToReadHead(Buffer* buffer) {
    assert(state_ == State::WAIT_HEAD);
    return buffer->readableBytes() > sizeof header_;
}

bool BasicParser::ableToReadData(Buffer* buffer) const {
    // TODO: maybe assert is unnecessary
    assert(state_ == State::WAIT_DATA);
    return static_cast<int>(buffer->readableBytes()) >= header_.data_length;
}
