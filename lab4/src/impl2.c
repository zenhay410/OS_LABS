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

    int min = (a < b) ? a : b;
    int g   = 1;

    for (int d = 1; d <= min; ++d) {
        if (a % d == 0 && b % d == 0) {
            g = d;
        }
    }
    return g;
}

float E(int x) {
    if (x < 0) {
        return NAN;
    }

    double sum  = 0.0;
    double fact = 1.0;

    for (int n = 0; n <= x; ++n) {
        if (n > 0) {
            fact *= (double)n;
        }
        sum += 1.0 / fact;
    }
    return (float)sum;
}
