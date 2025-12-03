#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <signal.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "shared.h"

static shared_data_t *shm = NULL;
static volatile sig_atomic_t sigusr1_flag = 0;

static void child1_sig_handler(int signo) {
    (void)signo;
    sigusr1_flag = 1;
}

// Перевод строки в нижний регистр
static void to_lower_inplace(char *s) {
    for (; *s; ++s) {
        *s = (char)tolower((unsigned char)*s);
    }
}

// Подключение к общей памяти
static void attach_shared(void) {
    int fd = open(SHM_FILE, O_RDWR);
    if (fd < 0) {
        perror("child1: open");
        exit(EXIT_FAILURE);
    }

    shm = mmap(NULL, sizeof(shared_data_t),
               PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shm == MAP_FAILED) {
        perror("child1: mmap");
        close(fd);
        exit(EXIT_FAILURE);
    }
    close(fd);
}

int main(void) {
    attach_shared();

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = child1_sig_handler;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGUSR1, &sa, NULL) < 0) {
        perror("child1: sigaction");
        exit(EXIT_FAILURE);
    }

    pid_t parent_pid = getppid();

    for (;;) {
        while (!sigusr1_flag) pause();
        sigusr1_flag = 0;

        if (shm->stage == 4) {
            break;
        }

        if (shm->stage == 1) {
            to_lower_inplace(shm->buf);
            shm->stage = 2;
            if (kill(parent_pid, SIGUSR2) < 0) {
                perror("child1: kill parent");
                break;
            }
        }
    }

    munmap(shm, sizeof(*shm));
    return 0;
}
