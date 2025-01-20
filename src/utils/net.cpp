#include "net.h"

#include <mutex>

#include <unistd.h>

static std::mutex sSocketMutex;

int32_t recvwait(const int32_t sock, void *buffer, int32_t len) {
    std::lock_guard lock(sSocketMutex);
    while (len > 0) {
        const int32_t ret = recv(sock, buffer, len, 0);
        if (ret < 0) {
            return ret;
        }
        len -= ret;
        buffer = (void *) (((char *) buffer) + ret);
    }
    return 0;
}

uint8_t recvbyte(const int32_t sock) {
    unsigned char buffer[1];
    int32_t ret;

    ret = recvwait(sock, buffer, 1);
    if (ret < 0)
        return ret;
    return buffer[0];
}

uint32_t recvword(const int32_t sock) {
    uint32_t result;
    int32_t ret;

    ret = recvwait(sock, &result, 4);
    if (ret < 0)
        return ret;
    return result;
}

int32_t checkbyte(const int32_t sock) {
    std::lock_guard lock(sSocketMutex);
    unsigned char buffer[1];

    const int32_t ret = recv(sock, buffer, 1, MSG_DONTWAIT);
    if (ret < 0)
        return ret;
    if (ret == 0)
        return -1;
    return buffer[0];
}

int32_t sendwait(int32_t sock, const void *buffer, int32_t len) {
    std::lock_guard lock(sSocketMutex);
    while (len > 0) {
        // For some reason the send blocks/crashes if the buffer is too big...
        const int cur_length = len <= 0x30 ? len : 0x30;
        const int32_t ret    = send(sock, buffer, cur_length, 0);
        if (ret < 0) {
            return ret;
        }
        len -= ret;
        buffer = (void *) (((char *) buffer) + ret);
    }
    return 0;
}

int32_t sendbyte(int32_t sock, unsigned char byte) {
    unsigned char buffer[1];
    buffer[0] = byte;
    return sendwait(sock, buffer, 1);
}
