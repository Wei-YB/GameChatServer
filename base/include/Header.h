#pragma once
#include <cstring>
#include <memory>

#include "Util.h"

namespace chatServer {

struct Header {
    Header() = default;

    explicit Header(const char* buffer);

    int   data_length;
    int   request_type;
    int   uid;
    int   stamp;
    char* fill(char* buffer) const;
    void  setFail(int status = -1) { request_type = status; }
    void  setOk() { request_type = 0; }

    [[nodiscard]]
    RequestType requestType() const {
        if ((request_type <= 0) || (request_type >= static_cast<int>(RequestType::MAX_REQUEST_TYPE))) {
            return RequestType::UNKNOWN;
        }
        return static_cast<RequestType>(request_type);
    }

    [[nodiscard]]
    bool status() const {
        return request_type == 0;
    }

    [[nodiscard]]
    int request_length() const { return data_length + static_cast<int>(sizeof(Header)); }

    // only for debug
    [[nodiscard]]
    std::string toString() const;
};

template <typename MessageTypePtr>
std::pair<char*, size_t> formatMessage(Header& header, std::shared_ptr<MessageTypePtr> data) {
    thread_local static char buffer[1024 * 8];

    auto str = data->SerializeAsString();
    auto data_length = str.size();
    header.data_length = data_length;
    auto next = header.fill(buffer);
    memcpy(next, str.c_str(), data_length);
    return {buffer, sizeof(Header) + data_length};
}

inline std::pair<char*, size_t> formatMessage(Header& header) {
    header.data_length = 0;
    thread_local static char buffer[16];
    header.fill(buffer);
    return {buffer, sizeof header};
}

inline std::pair<char*, size_t> formatMessage(Header& header, const char* str) {
    thread_local static char buffer[1024];
    const auto               next = header.fill(buffer);
    memcpy(next, str, header.data_length);
    return {buffer, header.request_length()};
}

}
