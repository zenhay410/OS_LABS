#include <stdio.h>
#include "calc.h"

int main(void) {
    char cmd;

    while (1) {
        if (scanf(" %c", &cmd) != 1) {
            break;
        }

        if (cmd == 'q' || cmd == 'Q') {
            break;
        } else if (cmd == '1') {
            int A, B;
            if (scanf("%d %d", &A, &B) != 2) {
                return 1;
            }
            int g = GCF(A, B);
            printf("%d\n", g);
        } else if (cmd == '2') {
            int x;
            if (scanf("%d", &x) != 1) {
                return 1;
            }
            float e_val = E(x);
            printf("%f\n", e_val);
        } else {
        }
    }

    return 0;
}
