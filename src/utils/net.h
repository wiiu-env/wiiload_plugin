#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

int32_t recvwait(int32_t sock, void *buffer, int32_t len);

uint8_t recvbyte(int32_t sock);

uint32_t recvword(int32_t sock);

int32_t checkbyte(int32_t sock);

int32_t sendwait(int32_t sock, const void *buffer, int32_t len);

int32_t sendbyte(int32_t sock, unsigned char byte);

#ifdef __cplusplus
}
#endif