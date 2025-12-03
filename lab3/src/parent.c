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
#include <sys/wait.h>

#include "shared.h"

static shared_data_t *shm = NULL;
static volatile sig_atomic_t sigusr2_flag = 0;

// Сигнал от дочерних процессов
static void parent_sig_handler(int signo) {
    (void)signo;
    sigusr2_flag = 1;
}

// Создание и отображение файла с общей памятью
static void create_shared(void) {
    int fd = open(SHM_FILE, O_RDWR | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) {
        perror("parent: open");
        exit(EXIT_FAILURE);
    }

    if (ftruncate(fd, sizeof(shared_data_t)) < 0) {
        perror("parent: ftruncate");
        close(fd);
        exit(EXIT_FAILURE);
    }

    shm = mmap(NULL, sizeof(shared_data_t),
               PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shm == MAP_FAILED) {
        perror("parent: mmap");
        close(fd);
        exit(EXIT_FAILURE);
    }
    close(fd);
    memset(shm, 0, sizeof(*shm));
}

int main(void) {
    create_shared();

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = parent_sig_handler;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGUSR2, &sa, NULL) < 0) {
        perror("parent: sigaction");
        exit(EXIT_FAILURE);
    }

    pid_t c1 = fork();
    if (c1 < 0) {
        perror("fork child1");
        exit(EXIT_FAILURE);
    }
    if (c1 == 0) {
        execl("./child1", "child1", NULL);
        perror("execl child1");
        _exit(EXIT_FAILURE);
    }

    pid_t c2 = fork();
    if (c2 < 0) {
        perror("fork child2");
        kill(c1, SIGKILL);
        exit(EXIT_FAILURE);
    }
    if (c2 == 0) {
        execl("./child2", "child2", NULL);
        perror("execl child2");
        _exit(EXIT_FAILURE);
    }

    // даём детям время поднять mmap и обработчики
    sleep(1);

    char line[BUF_SIZE];
    printf("Enter lines (Ctrl+D to finish):\n");

    while (fgets(line, sizeof(line), stdin) != NULL) {
        // шаг 1: parent -> child1
        strncpy(shm->buf, line, BUF_SIZE - 1);
        shm->buf[BUF_SIZE - 1] = '\0';

        shm->stage = 1;
        sigusr2_flag = 0;
        if (kill(c1, SIGUSR1) < 0) {
            perror("parent: kill child1");
            break;
        }
        while (!sigusr2_flag) pause();  // ждём завершения child1

        // шаг 2: child1 -> child2
        shm->stage = 2;
        sigusr2_flag = 0;
        if (kill(c2, SIGUSR1) < 0) {
            perror("parent: kill child2");
            break;
        }
        while (!sigusr2_flag) pause();  // ждём завершения child2

        if (shm->stage == 3) {
            fputs(shm->buf, stdout);
        }
    }

    // команда завершения
    shm->stage = 4;
    kill(c1, SIGUSR1);
    kill(c2, SIGUSR1);

    int status;
    waitpid(c1, &status, 0);
    waitpid(c2, &status, 0);

    munmap(shm, sizeof(*shm));
    unlink(SHM_FILE);

    return 0;
}
