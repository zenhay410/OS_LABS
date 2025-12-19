#include <zmq.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>

#include "protocol.h"

static void trim_line_end(char* s) {
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r')) {
        s[n - 1] = '\0';
        n--;
    }
}

static const char* skip_spaces(const char* p) {
    while (*p && isspace((unsigned char)*p)) p++;
    return p;
}

static int parse_token(const char** p, char* out, size_t out_sz) {
    const char* s = skip_spaces(*p);
    if (!*s) return 0;

    const char* start = s;
    while (*s && !isspace((unsigned char)*s)) s++;

    size_t len = (size_t)(s - start);
    if (len == 0) return 0;

    if (len >= out_sz) len = out_sz - 1;
    memcpy(out, start, len);
    out[len] = '\0';

    *p = s;
    return 1;
}

static int parse_rest(const char** p, char* out, size_t out_sz) {
    const char* s = skip_spaces(*p);
    if (!*s) return 0;
    safe_strcpy(out, out_sz, s);
    return 1;
}

static void print_prompt(void) {
    printf("> ");
    fflush(stdout);
}

static void print_help(void) {
    printf("Commands:\n");
    printf("  /to <user> <text>\n");
    printf("  /me <text>\n");
    printf("  /delay <sec> <user> <text>\n");
    printf("  /delayme <sec> <text>\n");
    printf("  /exit\n");
}

static void send_connect(void* socket, const char* login) {
    message_t m;
    memset(&m, 0, sizeof(m));
    m.type = MSG_CONNECT;
    safe_strcpy(m.from, LOGIN_LEN, login);
    zmq_send(socket, &m, sizeof(m), 0);
}

static void send_disconnect(void* socket, const char* login) {
    message_t m;
    memset(&m, 0, sizeof(m));
    m.type = MSG_DISCONNECT;
    safe_strcpy(m.from, LOGIN_LEN, login);
    zmq_send(socket, &m, sizeof(m), 0);
}

int main(int argc, char* argv[]) {
    setvbuf(stdout, NULL, _IONBF, 0);

    if (argc < 3 || strcmp(argv[1], "-u") != 0) {
        printf("Usage: %s -u <login>\n", argv[0]);
        return 1;
    }

    char login[LOGIN_LEN];
    safe_strcpy(login, LOGIN_LEN, argv[2]);

    void* ctx = zmq_ctx_new();
    void* socket = zmq_socket(ctx, ZMQ_DEALER);

    // Важно: пусть ZeroMQ сам переподключается после рестарта сервера
    zmq_connect(socket, "tcp://localhost:5555");

    // первичная регистрация
    send_connect(socket, login);

    char line[512];
    time_t last_announce = 0;

    print_prompt();

    while (1) {
        // раз в 2 секунды пере-регистрируемся (чтобы пережить рестарт сервера)
        time_t now = time(NULL);
        if (now - last_announce >= 2) {
            send_connect(socket, login);
            last_announce = now;
        }

        zmq_pollitem_t items[2];
        items[0].socket = socket;
        items[0].fd = 0;
        items[0].events = ZMQ_POLLIN;
        items[0].revents = 0;

        items[1].socket = NULL;
        items[1].fd = 0;              // stdin
        items[1].events = ZMQ_POLLIN;
        items[1].revents = 0;

        // 100мс — нормальный "реалтайм" для консоли
        int rc = zmq_poll(items, 2, 100);
        if (rc < 0) break;

        // входящие сообщения (реалтайм)
        if (items[0].revents & ZMQ_POLLIN) {
            message_t in;
            int n = zmq_recv(socket, &in, sizeof(in), 0);
            if (n == (int)sizeof(in)) {
                printf("\n[%s] %s\n", in.from, in.text);
                print_prompt();
            }
        }

        // ввод команд
        if (items[1].revents & ZMQ_POLLIN) {
            if (!fgets(line, sizeof(line), stdin)) break;
            trim_line_end(line);

            if (strcmp(line, "/exit") == 0) {
                send_disconnect(socket, login);
                break;
            }

            const char* p = line;
            message_t out;
            memset(&out, 0, sizeof(out));
            out.type = MSG_SEND;
            safe_strcpy(out.from, LOGIN_LEN, login);
            out.deliver_at = 0;

            if (strncmp(p, "/to", 3) == 0 && isspace((unsigned char)p[3])) {
                p += 3;
                if (!parse_token(&p, out.to, LOGIN_LEN) || !parse_rest(&p, out.text, TEXT_LEN)) {
                    printf("Format: /to <user> <text>\n");
                    print_prompt();
                    continue;
                }
            } else if (strncmp(p, "/me", 3) == 0 && isspace((unsigned char)p[3])) {
                p += 3;
                safe_strcpy(out.to, LOGIN_LEN, login);
                if (!parse_rest(&p, out.text, TEXT_LEN)) {
                    printf("Format: /me <text>\n");
                    print_prompt();
                    continue;
                }
            } else if (strncmp(p, "/delayme", 8) == 0 && isspace((unsigned char)p[8])) {
                p += 8;
                p = skip_spaces(p);
                char* endptr = NULL;
                unsigned long sec = strtoul(p, &endptr, 10);
                if (endptr == p) {
                    printf("Format: /delayme <sec> <text>\n");
                    print_prompt();
                    continue;
                }
                p = endptr;
                safe_strcpy(out.to, LOGIN_LEN, login);
                if (!parse_rest(&p, out.text, TEXT_LEN)) {
                    printf("Format: /delayme <sec> <text>\n");
                    print_prompt();
                    continue;
                }
                out.deliver_at = (uint64_t)time(NULL) + (uint64_t)sec;
            } else if (strncmp(p, "/delay", 6) == 0 && isspace((unsigned char)p[6])) {
                p += 6;
                p = skip_spaces(p);
                char* endptr = NULL;
                unsigned long sec = strtoul(p, &endptr, 10);
                if (endptr == p) {
                    printf("Format: /delay <sec> <user> <text>\n");
                    print_prompt();
                    continue;
                }
                p = endptr;
                if (!parse_token(&p, out.to, LOGIN_LEN) || !parse_rest(&p, out.text, TEXT_LEN)) {
                    printf("Format: /delay <sec> <user> <text>\n");
                    print_prompt();
                    continue;
                }
                out.deliver_at = (uint64_t)time(NULL) + (uint64_t)sec;
            } else {
                print_help();
                print_prompt();
                continue;
            }

            zmq_send(socket, &out, sizeof(out), 0);
            print_prompt();
        }
    }

    zmq_close(socket);
    zmq_ctx_destroy(ctx);
    return 0;
}
