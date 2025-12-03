#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <signal.h>
#include <string.h>
#include <errno.h>

#include "shared.h"

static shared_data_t *shm = NULL;
static volatile sig_atomic_t sigusr1_flag = 0;

static void child2_sig_handler(int signo) {
    (void)signo;
    sigusr1_flag = 1;
}

static void remove_double_spaces_inplace(char *s) {
    char *dst = s;
    int prev_space = 0;
    while (*s) {
        if (*s == ' ') {
            if (!prev_space) {
                *dst++ = *s;
                prev_space = 1;
            }
        } else {
            *dst++ = *s;
            prev_space = 0;
        }
        s++;
    }
    *dst = '\0';
}

static void attach_shared() {
    int fd = open(SHM_FILE, O_RDWR);
    if (fd < 0) {
        perror("child2: open shared file");
        exit(EXIT_FAILURE);
    }

    shm = mmap(NULL, sizeof(shared_data_t),
               PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shm == MAP_FAILED) {
        perror("child2: mmap");
        close(fd);
        exit(EXIT_FAILURE);
    }
    close(fd);
}

int main(void) {
    attach_shared();

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = child2_sig_handler;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGUSR1, &sa, NULL) < 0) {
        perror("child2: sigaction");
        exit(EXIT_FAILURE);
    }

    pid_t parent_pid = getppid();

    for (;;) {
        while (!sigusr1_flag) pause();
        sigusr1_flag = 0;

        if (shm->stage == 4) {
            break;
        }

        if (shm->stage == 2) {
            remove_double_spaces_inplace(shm->buf);
            shm->stage = 3;
            if (kill(parent_pid, SIGUSR2) < 0) {
                perror("child2: kill parent");
                break;
            }
        }
    }

    munmap(shm, sizeof(*shm));
    return 0;
}
