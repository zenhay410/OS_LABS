#include <stdio.h>
#include <unistd.h>
#include <ctype.h>

int main() {
    char buf[4096];
    ssize_t n;
    
    while ((n = read(STDIN_FILENO, buf, sizeof(buf))) > 0) {
        for (ssize_t i = 0; i < n; i++) {
            buf[i] = tolower((unsigned char)buf[i]);
        }
        write(STDOUT_FILENO, buf, n);
    }
    return 0;
}
