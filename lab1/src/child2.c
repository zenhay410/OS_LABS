#include <stdio.h>
#include <unistd.h>

int main() {
    char in[4096], out[4096];
    ssize_t n;
    int last_space = 0;

    while ((n = read(STDIN_FILENO, in, sizeof(in))) > 0) {
        ssize_t pos = 0;
        
        for (ssize_t i = 0; i < n; i++) {
            if (in[i] == ' ') {
                if (!last_space) {
                    out[pos++] = ' ';
                }
                last_space = 1;
            } else {
                out[pos++] = in[i];
                last_space = 0;
            }
        }
        write(STDOUT_FILENO, out, pos);
    }
    return 0;
}
