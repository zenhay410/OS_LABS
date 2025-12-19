#include <zmq.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#include "protocol.h"

#define MAX_CLIENTS 64
#define ID_MAX 256

#define SPOOL_DIR "./spool"
#define SCHED_FILE "./spool/_scheduled.bin"

#define POLL_MS 50

typedef struct {
    char login[LOGIN_LEN];
    unsigned char id[ID_MAX];
    size_t id_len;
    int used;
} client_t;

typedef struct delayed_msg {
    message_t msg;
    struct delayed_msg* next;
} delayed_msg_t;

static client_t clients[MAX_CLIENTS];
static delayed_msg_t* delayed_head = NULL;

static client_t* find_client(const char* login) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].used && strcmp(clients[i].login, login) == 0) {
            return &clients[i];
        }
    }
    return NULL;
}

static void upsert_client(const char* login, const unsigned char* id, size_t id_len) {
    client_t* c = find_client(login);

    if (!c) {
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (!clients[i].used) {
                c = &clients[i];
                c->used = 1;
                safe_strcpy(c->login, LOGIN_LEN, login);
                break;
            }
        }
    }

    if (!c) return;

    if (id_len > ID_MAX) id_len = ID_MAX;
    memcpy(c->id, id, id_len);
    c->id_len = id_len;
}

static void remove_client(const char* login) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].used && strcmp(clients[i].login, login) == 0) {
            clients[i].used = 0;
            clients[i].id_len = 0;
            clients[i].login[0] = '\0';
            return;
        }
    }
}

static void ensure_spool_dir(void) {
    if (mkdir(SPOOL_DIR, 0777) == -1) {
        if (errno != EEXIST) {
            perror("mkdir spool");
            exit(1);
        }
    }
}

static void save_offline(const char* user, const message_t* msg) {
    char path[256];
    snprintf(path, sizeof(path), SPOOL_DIR "/%s.bin", user);

    FILE* f = fopen(path, "ab");
    if (!f) return;
    fwrite(msg, sizeof(*msg), 1, f);
    fclose(f);
}

static void deliver_offline(void* router, const client_t* client) {
    char path[256];
    snprintf(path, sizeof(path), SPOOL_DIR "/%s.bin", client->login);

    FILE* f = fopen(path, "rb");
    if (!f) return;

    message_t msg;
    while (fread(&msg, sizeof(msg), 1, f) == 1) {
        zmq_send(router, client->id, client->id_len, ZMQ_SNDMORE);
        zmq_send(router, &msg, sizeof(msg), 0);
    }

    fclose(f);
    remove(path);
}

static void rewrite_scheduled_file(void) {
    FILE* f = fopen(SCHED_FILE, "wb");
    if (!f) return;

    delayed_msg_t* cur = delayed_head;
    while (cur) {
        fwrite(&cur->msg, sizeof(cur->msg), 1, f);
        cur = cur->next;
    }

    fclose(f);
}

static void push_scheduled(const message_t* msg) {
    delayed_msg_t* node = (delayed_msg_t*)malloc(sizeof(delayed_msg_t));
    if (!node) return;

    node->msg = *msg;
    node->next = delayed_head;
    delayed_head = node;

    rewrite_scheduled_file();
}

static void load_scheduled_from_disk(void) {
    FILE* f = fopen(SCHED_FILE, "rb");
    if (!f) return;

    message_t msg;
    while (fread(&msg, sizeof(msg), 1, f) == 1) {
        delayed_msg_t* node = (delayed_msg_t*)malloc(sizeof(delayed_msg_t));
        if (!node) break;

        node->msg = msg;
        node->next = delayed_head;
        delayed_head = node;
    }

    fclose(f);
}

static void send_to_user(void* router, const char* to_login, const message_t* msg) {
    client_t* to = find_client(to_login);
    if (to) {
        zmq_send(router, to->id, to->id_len, ZMQ_SNDMORE);
        zmq_send(router, msg, sizeof(*msg), 0);
    } else {
        save_offline(to_login, msg);
    }
}

static void process_due(void* router) {
    time_t now = time(NULL);

    delayed_msg_t** cur = &delayed_head;
    int changed = 0;

    while (*cur) {
        if ((*cur)->msg.deliver_at != 0 &&
            (*cur)->msg.deliver_at <= (uint64_t)now) {

            message_t out = (*cur)->msg;
            out.type = MSG_DELIVER;

            send_to_user(router, out.to, &out);

            delayed_msg_t* done = *cur;
            *cur = (*cur)->next;
            free(done);

            changed = 1;
        } else {
            cur = &(*cur)->next;
        }
    }

    if (changed) {
        rewrite_scheduled_file();
    }
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);

    ensure_spool_dir();
    load_scheduled_from_disk();

    void* ctx = zmq_ctx_new();
    void* router = zmq_socket(ctx, ZMQ_ROUTER);

    if (zmq_bind(router, "tcp://*:5555") != 0) {
        perror("zmq_bind");
        zmq_close(router);
        zmq_ctx_destroy(ctx);
        return 1;
    }

    printf("Server started on tcp://*:5555\n");

    zmq_pollitem_t items[] = {
        { router, 0, ZMQ_POLLIN, 0 }
    };

    while (1) {
        int rc = zmq_poll(items, 1, POLL_MS);
        if (rc < 0) break;

        process_due(router);

        if (!(items[0].revents & ZMQ_POLLIN)) {
            continue;
        }

        zmq_msg_t id_msg;
        zmq_msg_init(&id_msg);

        if (zmq_msg_recv(&id_msg, router, 0) == -1) {
            zmq_msg_close(&id_msg);
            continue;
        }

        unsigned char id_buf[ID_MAX];
        size_t id_len = zmq_msg_size(&id_msg);
        if (id_len > ID_MAX) id_len = ID_MAX;
        memcpy(id_buf, zmq_msg_data(&id_msg), id_len);

        zmq_msg_close(&id_msg);

        message_t msg;
        int n = zmq_recv(router, &msg, sizeof(msg), 0);
        if (n != (int)sizeof(msg)) {
            continue;
        }

        if (msg.type == MSG_CONNECT) {
            upsert_client(msg.from, id_buf, id_len);
            client_t* c = find_client(msg.from);
            if (c) {
                deliver_offline(router, c);
            }
            continue;
        }

        if (msg.type == MSG_DISCONNECT) {
            remove_client(msg.from);
            continue;
        }

        if (msg.type == MSG_SEND) {
            time_t now = time(NULL);

            if (msg.deliver_at != 0 && msg.deliver_at > (uint64_t)now) {
                push_scheduled(&msg);
            } else {
                message_t out = msg;
                out.type = MSG_DELIVER;
                send_to_user(router, out.to, &out);
            }
        }
    }

    zmq_close(router);
    zmq_ctx_destroy(ctx);
    return 0;
}
