/* msort.c — параллельная сортировка слиянием массива 10^7 элементов.
 * СРС 1, задание 3, вариант 8. Синхронизация — только pthread_join.
 *
 * Рекурсия: на каждом уровне, пока depth > 0, левая половина сортируется
 * в новом потоке, правая — в текущем; затем pthread_join и слияние.
 * Глубина d даёт 2^d листовых задач, т.е. 1, 2, 4, 8 потоков при d = 0..3.
 * Мелкие куски (< CUTOFF) сортируются вставками.
 *
 * Использование: ./msort [N] [потоков: 1|2|4|8|...] [повторов]
 *                ./msort bench [N] [повторов]  — таблица для 1,2,4,8 потоков */
#define _POSIX_C_SOURCE 200809L
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define CUTOFF 32

struct task {
    int *a, *tmp;             /* массив и буфер того же размера */
    size_t lo, hi;            /* полуинтервал [lo, hi)           */
    int depth;                /* сколько ещё уровней порождать потоки */
};

static void insertion(int *a, size_t lo, size_t hi)
{
    for (size_t i = lo + 1; i < hi; i++) {
        int x = a[i];
        size_t j = i;
        while (j > lo && a[j - 1] > x) { a[j] = a[j - 1]; j--; }
        a[j] = x;
    }
}

static void merge(int *a, int *tmp, size_t lo, size_t mid, size_t hi)
{
    size_t i = lo, j = mid, k = lo;
    while (i < mid && j < hi) tmp[k++] = (a[i] <= a[j]) ? a[i++] : a[j++];
    while (i < mid) tmp[k++] = a[i++];
    while (j < hi)  tmp[k++] = a[j++];
    memcpy(a + lo, tmp + lo, (hi - lo) * sizeof *a);
}

static void msort_seq(int *a, int *tmp, size_t lo, size_t hi)
{
    if (hi - lo <= CUTOFF) { insertion(a, lo, hi); return; }
    size_t mid = lo + (hi - lo) / 2;
    msort_seq(a, tmp, lo, mid);
    msort_seq(a, tmp, mid, hi);
    if (a[mid - 1] <= a[mid]) return;        /* уже упорядочено */
    merge(a, tmp, lo, mid, hi);
}

static void *msort_par(void *arg)
{
    struct task *t = arg;
    if (t->depth <= 0 || t->hi - t->lo <= CUTOFF) {
        msort_seq(t->a, t->tmp, t->lo, t->hi);
        return NULL;
    }
    size_t mid = t->lo + (t->hi - t->lo) / 2;
    struct task left  = { t->a, t->tmp, t->lo, mid,   t->depth - 1 };
    struct task right = { t->a, t->tmp, mid,   t->hi, t->depth - 1 };

    pthread_t th;
    int err = pthread_create(&th, NULL, msort_par, &left);
    if (err) {                                 /* не удалось — сортируем сами */
        fprintf(stderr, "pthread_create: %s, продолжаю последовательно\n", strerror(err));
        msort_par(&left);
    }
    msort_par(&right);                         /* правая половина — в текущем потоке */
    if (!err) {
        err = pthread_join(th, NULL);          /* ждём левую половину */
        if (err) { fprintf(stderr, "pthread_join: %s\n", strerror(err)); exit(1); }
    }
    if (t->a[mid - 1] > t->a[mid]) merge(t->a, t->tmp, t->lo, mid, t->hi);
    return NULL;
}

static double now(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

static int depth_for(int threads)
{
    int d = 0;
    while ((1 << d) < threads) d++;
    return d;
}

static int is_sorted(const int *a, size_t n)
{
    for (size_t i = 1; i < n; i++) if (a[i - 1] > a[i]) return 0;
    return 1;
}

/* Отсортировать копию исходных данных, вернуть время в секундах */
static double one_run(const int *src, int *a, int *tmp, size_t n, int threads)
{
    memcpy(a, src, n * sizeof *a);
    struct task root = { a, tmp, 0, n, depth_for(threads) };
    double t0 = now();
    msort_par(&root);
    double t = now() - t0;
    if (!is_sorted(a, n)) { fprintf(stderr, "ОШИБКА: массив не отсортирован!\n"); exit(1); }
    return t;
}

static int cmp_int(const void *x, const void *y)
{
    int a = *(const int *)x, b = *(const int *)y;
    return (a > b) - (a < b);
}

int main(int argc, char *argv[])
{
    int bench = argc > 1 && strcmp(argv[1], "bench") == 0;
    int ai = bench ? 2 : 1;
    size_t n   = argc > ai     ? strtoull(argv[ai], NULL, 10) : 10000000;
    int threads = (!bench && argc > ai + 1) ? atoi(argv[ai + 1]) : 1;
    int reps   = argc > ai + (bench ? 1 : 2) ? atoi(argv[ai + (bench ? 1 : 2)]) : 3;
    if (n < 2) n = 2;
    if (threads < 1) threads = 1;
    if (reps < 1) reps = 1;

    int *src = malloc(n * sizeof *src), *a = malloc(n * sizeof *a), *tmp = malloc(n * sizeof *tmp);
    if (!src || !a || !tmp) { perror("malloc"); return 1; }
    srand(12345);
    for (size_t i = 0; i < n; i++) src[i] = rand();

    if (!bench) {
        double best = 1e9;
        for (int r = 0; r < reps; r++) {
            double t = one_run(src, a, tmp, n, threads);
            printf("N=%zu потоков=%d (глубина %d) запуск %d: %.3f с\n", n, threads, depth_for(threads), r + 1, t);
            if (t < best) best = t;
        }
        printf("лучшее время: %.3f с, массив отсортирован корректно\n", best);
    } else {
        memcpy(a, src, n * sizeof *a);
        double t0 = now();
        qsort(a, n, sizeof *a, cmp_int);
        printf("qsort (эталон, 1 поток): %.3f с\n", now() - t0);

        int tlist[] = { 1, 2, 4, 8 };
        double base = 0;
        printf("потоков,время_с,ускорение,эффективность\n");
        for (int k = 0; k < 4; k++) {
            double best = 1e9;
            for (int r = 0; r < reps; r++) {
                double t = one_run(src, a, tmp, n, tlist[k]);
                if (t < best) best = t;
            }
            if (k == 0) base = best;
            printf("%d,%.3f,%.2f,%.2f\n", tlist[k], best, base / best, base / best / tlist[k]);
        }
    }
    free(src); free(a); free(tmp);
    return 0;
}
