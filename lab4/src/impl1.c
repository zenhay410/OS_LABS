#include <math.h>
#include "calc.h"

static int abs_int(int x) {
    return x < 0 ? -x : x;
}

int GCF(int A, int B) {
    int a = abs_int(A);
    int b = abs_int(B);

    if (a == 0) return b;
    if (b == 0) return a;

    while (b != 0) {
        int t = a % b;
        a = b;
        b = t;
    }
    return a;
}

float E(int x) {
    if (x <= 0) {
        return NAN;
    }

    double base = 1.0 + 1.0 / (double)x;
    double val  = pow(base, (double)x);
    return (float)val;
}
