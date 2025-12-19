#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stdio.h>

#define LOGIN_LEN 32
#define TEXT_LEN  256

static inline void safe_strcpy(char* dst, size_t dst_size, const char* src) {
    if (dst_size == 0) return;
    snprintf(dst, dst_size, "%s", src);
}

typedef enum {
    MSG_CONNECT = 1,
    MSG_DISCONNECT = 2,
    MSG_SEND = 3,
    MSG_DELIVER = 4
} msg_type_t;

typedef struct {
    msg_type_t type;
    char from[LOGIN_LEN];
    char to[LOGIN_LEN];
    uint64_t deliver_at;   // unix time (seconds). 0 = send now
    char text[TEXT_LEN];
} message_t;

#endif
