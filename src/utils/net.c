#include "net.h"
#include <unistd.h>

static volatile int socket_lock __attribute__((section(".data"))) = 0;

int32_t recvwait(int32_t sock, void *buffer, int32_t len) {
    while(socket_lock) {
        usleep(1000);
    }
    int32_t ret;
    while (len > 0) {
        ret = recv(sock, buffer, len, 0);
        if(ret < 0) {
            socket_lock = 0;
            return ret;
        }
        len -= ret;
        buffer =  (void *)(((char *) buffer) + ret);
    }
    socket_lock = 0;
    return 0;
}

uint8_t recvbyte(int32_t sock) {
    unsigned char buffer[1];
    int32_t ret;

    ret = recvwait(sock, buffer, 1);
    if (ret < 0)
        return ret;
    return buffer[0];
}

uint32_t recvword(int32_t sock) {
    uint32_t result;
    int32_t ret;

    ret = recvwait(sock, &result, 4);
    if (ret < 0)
        return ret;
    return result;
}

int32_t checkbyte(int32_t sock) {
    while(socket_lock) {
        usleep(1000);
    }
    unsigned char buffer[1];
    int32_t ret;

    ret = recv(sock, buffer, 1, MSG_DONTWAIT);
    socket_lock = 0;
    if (ret < 0)
        return ret;
    if (ret == 0)
        return -1;
    return buffer[0];
}

int32_t sendwait(int32_t sock, const void *buffer, int32_t len) {
    while(socket_lock) {
        usleep(1000);
    }
    int32_t ret;
    while (len > 0) {
        // For some reason the send blocks/crashes if the buffer is too big..
        int cur_length = len <= 0x30 ? len : 0x30;
        ret = send(sock, buffer, cur_length, 0);
        if(ret < 0) {
            socket_lock = 0;
            return ret;
        }
        len -= ret;
        buffer =  (void *)(((char *) buffer) + ret);
    }
    socket_lock = 0;
    return 0;
}

int32_t sendbyte(int32_t sock, unsigned char byte) {
    unsigned char buffer[1];
    buffer[0] = byte;
    return sendwait(sock, buffer, 1);
}
