#ifndef _UTILS_NET_H_
#define _UTILS_NET_H_
#include <stdint.h>
#include <nsysnet/socket.h>
#ifdef __cplusplus
extern "C" {
#endif

int32_t recvwait(int32_t sock, void *buffer, int32_t len);
uint8_t recvbyte(int32_t sock);
uint32_t recvword(int32_t sock);
int32_t checkbyte(int32_t sock);
int32_t sendwait(int32_t sock, const void *buffer, int32_t len);
int32_t sendbyte(int32_t sock, unsigned char byte);

#ifdef __cplusplus
}
#endif

#endif
