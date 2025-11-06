#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>

#define BUF 4096

ssize_t write_all(int fd, const void *buf, size_t count) {
    const char *p = buf;
    size_t left = count;
    while (left > 0) {
        ssize_t n = write(fd, p, left);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        left -= n;
        p += n;
    }
    return count;
}

ssize_t read_line(int fd, char *buf, size_t cap) {
    size_t pos = 0;
    while (pos + 1 < cap) {
        char c;
        ssize_t n = read(fd, &c, 1);
        if (n <= 0) break;
        buf[pos++] = c;
        if (c == '\n') break;
    }
    buf[pos] = '\0';
    return pos;
}

int main() {
    int p1[2], p2[2], p3[2];

    if (pipe(p1) == -1 || pipe(p2) == -1 || pipe(p3) == -1) {
        perror("pipe");
        exit(1);
    }

    pid_t c1 = fork();
    if (c1 == 0) {
        dup2(p1[0], STDIN_FILENO);
        dup2(p2[1], STDOUT_FILENO);

        close(p1[0]); close(p1[1]);
        close(p2[0]); close(p2[1]);
        close(p3[0]); close(p3[1]);

        execl("./child1", "child1", NULL);
        perror("exec child1");
        _exit(1);
    }

    pid_t c2 = fork();
    if (c2 == 0) {
        dup2(p2[0], STDIN_FILENO);
        dup2(p3[1], STDOUT_FILENO);

        close(p1[0]); close(p1[1]);
        close(p2[0]); close(p2[1]);
        close(p3[0]); close(p3[1]);

        execl("./child2", "child2", NULL);
        perror("exec child2");
        _exit(1);
    }

    close(p1[0]);
    close(p2[0]); close(p2[1]);
    close(p3[1]);

    char line[BUF], result[BUF];
    printf("Введите строку:\n");

    while (fgets(line, sizeof(line), stdin)) {
        write_all(p1[1], line, strlen(line));
        
        ssize_t n = read_line(p3[0], result, sizeof(result));
        if (n <= 0) break;
        
        printf("Результат: %s", result);
    }

    close(p1[1]);
    close(p3[0]);
    waitpid(c1, NULL, 0);
    waitpid(c2, NULL, 0);
    
    return 0;
}
