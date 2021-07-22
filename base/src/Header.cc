#include "Header.h"

#include <cstring>

using namespace chatServer;

Header::Header(const char* buffer): Header() {
    memcpy(this, buffer, sizeof(Header));
}

char* Header::fill(char* buffer) const {
    memcpy(buffer, this, sizeof (Header));
    return buffer + sizeof(Header);
}

std::string Header::toString() const {
    char buffer[128];
    bzero(buffer, sizeof buffer);
    snprintf(buffer, sizeof buffer, "len = %d, type = %d, uid = %d, stamp = %d", data_length, request_type, uid,
             stamp);
    return buffer;
}
