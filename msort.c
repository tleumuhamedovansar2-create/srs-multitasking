/* race.c — общая часть задания 3: гонка на счётчике и три способа исправления.
 * 4 потока по 10^6 инкрементов. Режимы: none | mutex | atomic | local | all
 * Использование: ./race [режим] [число_запусков]                              */
#define _POSIX_C_SOURCE 200809L
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define NTHREADS 4
#define NITER    1000000

/* volatile: запрещаем компилятору при -O2 заменить цикл одним сложением,
 * каждый инкремент — реальные чтение и запись в память */
static volatile long   counter = 0;        /* общий счётчик (none, mutex, local) */
static atomic_long     acounter = 0;       /* атомарный счётчик                  */
static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;

static void *worker_none(void *arg)
{
    (void)arg;
    for (int i = 0; i < NITER; i++)
        counter++;                          /* load-add-store без защиты: ГОНКА */
    return NULL;
}

static void *worker_mutex(void *arg)
{
    (void)arg;
    for (int i = 0; i < NITER; i++) {
        pthread_mutex_lock(&mtx);
        counter++;
        pthread_mutex_unlock(&mtx);
    }
    return NULL;
}

static void *worker_atomic(void *arg)
{
    (void)arg;
    for (int i = 0; i < NITER; i++)
        atomic_fetch_add_explicit(&acounter, 1, memory_order_relaxed);
    return NULL;
}

static void *worker_local(void *arg)
{
    (void)arg;
    volatile long local = 0;                /* volatile — чтобы цикл не выбросил оптимизатор */
    for (int i = 0; i < NITER; i++)
        local++;                            /* считаем у себя */
    pthread_mutex_lock(&mtx);               /* одна защищённая операция на поток */
    counter += local;
    pthread_mutex_unlock(&mtx);
    return NULL;
}

static double now(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

static void run(const char *name, void *(*fn)(void *), int is_atomic)
{
    pthread_t th[NTHREADS];
    counter = 0;
    atomic_store(&acounter, 0);
    double t0 = now();
    for (int i = 0; i < NTHREADS; i++) {
        int err = pthread_create(&th[i], NULL, fn, NULL);
        if (err) { fprintf(stderr, "pthread_create: %s\n", strerror(err)); exit(1); }
    }
    for (int i = 0; i < NTHREADS; i++) {
        int err = pthread_join(th[i], NULL);
        if (err) { fprintf(stderr, "pthread_join: %s\n", strerror(err)); exit(1); }
    }
    double t = now() - t0;
    long result = is_atomic ? atomic_load(&acounter) : counter;
    long expected = (long)NTHREADS * NITER;
    printf("%-7s результат = %8ld из %ld  потеряно = %8ld  время = %7.2f мс\n",
           name, result, expected, expected - result, t * 1000);
}

int main(int argc, char *argv[])
{
    const char *mode = argc > 1 ? argv[1] : "all";
    int runs = argc > 2 ? atoi(argv[2]) : 5;
    if (runs < 1) runs = 1;

    for (int r = 1; r <= runs; r++) {
        if (runs > 1) printf("--- запуск %d ---\n", r);
        if (!strcmp(mode, "none")   || !strcmp(mode, "all")) run("none",   worker_none,   0);
        if (!strcmp(mode, "mutex")  || !strcmp(mode, "all")) run("mutex",  worker_mutex,  0);
        if (!strcmp(mode, "atomic") || !strcmp(mode, "all")) run("atomic", worker_atomic, 1);
        if (!strcmp(mode, "local")  || !strcmp(mode, "all")) run("local",  worker_local,  0);
    }
    return 0;
}
