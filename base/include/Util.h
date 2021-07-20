#pragma once

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