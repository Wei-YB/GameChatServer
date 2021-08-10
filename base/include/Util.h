#pragma once

#include <chrono>


namespace chatServer {
inline int now() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

enum class RequestType {
    LOGIN = 1,
    ONLINE = 2,
    OFFLINE = 3,
    SIGN_UP = 4,
    GET_BLACK_LIST = 5,
    ADD_BLACK_LIST = 6,
    DEL_BLACK_LIST = 7,
    IS_BLACK_LIST = 8,
    INFO = 9,
    CHAT = 10,
    BROADCAST_AREA = 11,
    BROADCAST_ALL = 12,
    LOGOUT = 13,
    PLAYER_LIST = 14,
    MAX_REQUEST_TYPE,
    UNKNOWN,
};
}
