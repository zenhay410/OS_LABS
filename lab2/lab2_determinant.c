#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

int MAX_THREADS = 4;

typedef struct {
    int id;
    int n;
    double *matrix;
    long long start;
    long long end;
    double partial_sum;
} ThreadData;

long long factorial_ll(int n) {
    long long f = 1;
    for (int i = 2; i <= n; i++) {
        f *= i;
    }
    return f;
}

// знак перестановки по числу инверсий
int sign_of_perm(const int *p, int n) {
    int inv = 0;
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            if (p[i] > p[j]) {
                inv++;
            }
        }
    }
    return (inv % 2 == 0) ? 1 : -1;
}

// номер перестановки -> сама перестановка (лексикографический порядок)
void index_to_perm(long long idx, int *p, int n) {
    int used[20] = {0}; // n <= 20

    for (int i = 0; i < n; i++) {
        int k = idx % (n - i);
        idx /= (n - i);

        int cnt = 0;
        for (int j = 0; j < n; j++) {
            if (!used[j]) {
                if (cnt == k) {
                    p[i] = j;
                    used[j] = 1;
                    break;
                }
                cnt++;
            }
        }
    }
}

// рабочий поток: считает часть суммы по перестановкам
void *det_worker(void *arg) {
    ThreadData *data = (ThreadData *)arg;
    int n = data->n;
    double *A = data->matrix;
    int perm[20];
    double sum = 0.0;

    for (long long idx = data->start; idx < data->end; idx++) {
        index_to_perm(idx, perm, n);

        double prod = 1.0;
        for (int r = 0; r < n; r++) {
            prod *= A[r * n + perm[r]];
        }

        int s = sign_of_perm(perm, n);
        sum += s * prod;
    }

    data->partial_sum = sum;
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <max_threads> <matrix_size>\n", argv[0]);
        return 1;
    }

    MAX_THREADS = atoi(argv[1]);
    int n = atoi(argv[2]);

    if (n <= 0 || n > 10) {
        printf("Matrix size n must be between 1 and 10\n");
        return 1;
    }

    if (MAX_THREADS <= 0) {
        printf("Thread count must be positive\n");
        return 1;
    }

    // матрица n×n, для простоты заполняем псевдослучайно, но детерминированно
    double *A = malloc(n * n * sizeof(double));
    if (!A) {
        perror("malloc");
        return 1;
    }

    srand(42);
    for (int i = 0; i < n * n; i++) {
        A[i] = (rand() % 11) - 5; // [-5; 5]
    }

    long long total_perms = factorial_ll(n);

    int actual_threads = MAX_THREADS;
    if (actual_threads > total_perms) {
        actual_threads = (int)total_perms;
    }

    pthread_t *threads = malloc(sizeof(pthread_t) * actual_threads);
    ThreadData *thread_data = malloc(sizeof(ThreadData) * actual_threads);
    if (!threads || !thread_data) {
        perror("malloc");
        free(A);
        return 1;
    }

    long long base = total_perms / actual_threads;
    long long rem  = total_perms % actual_threads;

    struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    long long current = 0;
    for (int i = 0; i < actual_threads; i++) {
        long long chunk = base + (i < rem ? 1 : 0);

        thread_data[i].id          = i;
        thread_data[i].n           = n;
        thread_data[i].matrix      = A;
        thread_data[i].start       = current;
        thread_data[i].end         = current + chunk;
        thread_data[i].partial_sum = 0.0;

        current += chunk;

        pthread_create(&threads[i], NULL, det_worker, &thread_data[i]);
    }

    double det = 0.0;
    for (int i = 0; i < actual_threads; i++) {
        pthread_join(threads[i], NULL);
        det += thread_data[i].partial_sum;
    }

    clock_gettime(CLOCK_MONOTONIC, &end_time);

    double time_sec = (end_time.tv_sec - start_time.tv_sec) +
                      (end_time.tv_nsec - start_time.tv_nsec) / 1e9;

    printf("Matrix size: %d\n", n);
    printf("Determinant: %.4f\n", det);
    printf("Time: %lf seconds\n", time_sec);
    printf("Threads used: %d (max: %d)\n", actual_threads, MAX_THREADS);
    printf("Process PID: %d\n", getpid());

    free(A);
    free(threads);
    free(thread_data);

    return 0;
}
