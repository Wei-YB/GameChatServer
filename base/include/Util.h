#pragma once

#include <chrono>

namespace chatServer::database {
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
        MAX_REQUEST_TYPE,
        UNKNOWN,
    };
}

namespace chatServer {
inline void fillHead(const int (&head)[3], char* buffer) {
    memcpy(buffer, head, sizeof(int[3]));
}

inline void fillHead(int length, int type, int uid, char* buffer) {
    fillHead({length, type, uid}, buffer);
}

inline int now() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

}
