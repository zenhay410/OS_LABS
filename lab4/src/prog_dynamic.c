#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include "calc.h"

typedef int   (*GCF_fn_t)(int, int);
typedef float (*E_fn_t)(int);

struct Impl {
    void    *handle;
    GCF_fn_t GCF;
    E_fn_t   E;
    const char *path;
};

int main(void) {
    struct Impl impls[2] = {
        {NULL, NULL, NULL, "./libimpl1.so"},
        {NULL, NULL, NULL, "./libimpl2.so"}
    };

    for (int i = 0; i < 2; ++i) {
        impls[i].handle = dlopen(impls[i].path, RTLD_LAZY);
        if (!impls[i].handle) {
            fprintf(stderr, "dlopen(%s) failed: %s\n",
                    impls[i].path, dlerror());
            for (int j = 0; j < i; ++j) {
                dlclose(impls[j].handle);
            }
            return 1;
        }

        dlerror();

        impls[i].GCF = (GCF_fn_t)dlsym(impls[i].handle, "GCF");
        const char *err = dlerror();
        if (err) {
            fprintf(stderr, "dlsym(GCF) in %s failed: %s\n",
                    impls[i].path, err);
            for (int j = 0; j <= i; ++j) {
                dlclose(impls[j].handle);
            }
            return 1;
        }

        impls[i].E = (E_fn_t)dlsym(impls[i].handle, "E");
        err = dlerror();
        if (err) {
            fprintf(stderr, "dlsym(E) in %s failed: %s\n",
                    impls[i].path, err);
            for (int j = 0; j <= i; ++j) {
                dlclose(impls[j].handle);
            }
            return 1;
        }
    }

    int current = 0;
    char cmd;

    while (1) {
        if (scanf(" %c", &cmd) != 1) {
            break;
        }

        if (cmd == 'q' || cmd == 'Q') {
            break;
        } else if (cmd == '0') {
            current = 1 - current;
        } else if (cmd == '1') {
            int A, B;
            if (scanf("%d %d", &A, &B) != 2) {
                break;
            }
            int g = impls[current].GCF(A, B);
            printf("%d\n", g);
        } else if (cmd == '2') {
            int x;
            if (scanf("%d", &x) != 1) {
                break;
            }
            float e_val = impls[current].E(x);
            printf("%f\n", e_val);
        } else {
        }
    }

    for (int i = 0; i < 2; ++i) {
        if (impls[i].handle) {
            dlclose(impls[i].handle);
        }
    }

    return 0;
}
