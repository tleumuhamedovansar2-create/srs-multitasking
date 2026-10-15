/* server.c — сервер вычисления выражений на FIFO. СРС 1, задание 2, вариант 8.
 *
 * Схема: клиенты пишут запросы фиксированного размера в общий FIFO SERVER_FIFO.
 * Сервер (родитель) читает запрос и порождает fork() обработчик; обработчик
 * вычисляет выражение, открывает личный FIFO клиента и пишет ответ.
 * Завершившиеся обработчики собираются по SIGCHLD через waitpid(WNOHANG) —
 * зомби не накапливаются. По SIGINT/SIGTERM сервер перестаёт принимать
 * запросы, дожидается всех потомков и удаляет FIFO.
 *
 * Использование: ./server [-d задержка_мс] [-n число_запросов] */
#define _POSIX_C_SOURCE 200809L
#include "common.h"
#include "calc.h"
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t got_chld = 0;
static volatile sig_atomic_t stop     = 0;
static int active = 0;           /* число работающих обработчиков */
static int served = 0, failed = 0;

static void on_chld(int sig) { (void)sig; got_chld = 1; }
static void on_stop(int sig) { (void)sig; stop = 1; }

/* Собрать всех завершившихся потомков, вывести PID и код завершения */
static void reap(int options)
{
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, options)) > 0) {
        active--;
        if (WIFEXITED(status)) {
            int code = WEXITSTATUS(status);
            printf("[server %d] обработчик PID=%d завершился, код=%d%s\n",
                   (int)getpid(), (int)pid, code, code == 0 ? "" : " (ошибка)");
            if (code == 0) served++; else failed++;
        } else if (WIFSIGNALED(status)) {
            printf("[server %d] обработчик PID=%d убит сигналом %d\n",
                   (int)getpid(), (int)pid, WTERMSIG(status));
            failed++;
        }
    }
    if (pid == -1 && errno != ECHILD) perror("waitpid");
    fflush(stdout);
}

/* Код обработчика (выполняется в потомке). Возвращает код завершения. */
static int handle(const struct request *rq, int delay_ms)
{
    struct answer an;
    memset(&an, 0, sizeof an);
    an.worker_pid = getpid();

    if (delay_ms > 0) {                       /* имитация «долгого» вычисления */
        struct timespec ts = { delay_ms / 1000, (delay_ms % 1000) * 1000000L };
        nanosleep(&ts, NULL);
    }
    char err[ANSWER_MAX];
    if (calc_eval(rq->expr, &an.value, err, sizeof err) == 0) {
        an.ok = 1;
        snprintf(an.text, sizeof an.text, "%s = %.10g", rq->expr, an.value);
    } else {
        an.ok = 0;
        snprintf(an.text, sizeof an.text, "%s", err);
    }

    char path[64];
    snprintf(path, sizeof path, CLIENT_FIFO_FMT, (int)rq->client_pid);
    int fd = open(path, O_WRONLY);
    if (fd == -1) { fprintf(stderr, "[worker %d] open %s: %s\n", (int)getpid(), path, strerror(errno)); return 2; }
    if (write(fd, &an, sizeof an) != (ssize_t)sizeof an) { perror("write answer"); close(fd); return 3; }
    if (close(fd) == -1) { perror("close"); return 3; }
    printf("[worker %d] клиент %d: %s\n", (int)getpid(), (int)rq->client_pid, an.text);
    return an.ok ? 0 : 1;
}

int main(int argc, char *argv[])
{
    int delay_ms = 0, limit = -1, opt;
    while ((opt = getopt(argc, argv, "d:n:")) != -1) {
        if (opt == 'd') delay_ms = atoi(optarg);
        else if (opt == 'n') limit = atoi(optarg);
        else { fprintf(stderr, "Использование: %s [-d мс] [-n запросов]\n", argv[0]); return 1; }
    }
    setvbuf(stdout, NULL, _IOLBF, 0);

    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = on_chld;
    sa.sa_flags = SA_NOCLDSTOP;               /* без SA_RESTART: read прервётся EINTR */
    if (sigaction(SIGCHLD, &sa, NULL) == -1) { perror("sigaction SIGCHLD"); return 1; }
    sa.sa_handler = on_stop;
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, NULL) == -1 || sigaction(SIGTERM, &sa, NULL) == -1) {
        perror("sigaction SIGINT/SIGTERM"); return 1;
    }
    sa.sa_handler = SIG_IGN;                  /* клиент мог уйти — не умирать от SIGPIPE */
    if (sigaction(SIGPIPE, &sa, NULL) == -1) { perror("sigaction SIGPIPE"); return 1; }

    umask(0);
    if (mkfifo(SERVER_FIFO, 0666) == -1 && errno != EEXIST) { perror("mkfifo " SERVER_FIFO); return 1; }

    printf("[server %d] ожидаю запросы в %s (задержка обработки %d мс)\n", (int)getpid(), SERVER_FIFO, delay_ms);
    int rfd = open(SERVER_FIFO, O_RDONLY);    /* блокируется до первого клиента */
    if (rfd == -1) {
        if (errno == EINTR) { unlink(SERVER_FIFO); return 0; }
        perror("open server fifo (r)"); return 1;
    }
    /* Свой дескриптор на запись: когда последний клиент закроет FIFO,
     * read() не вернёт 0 (EOF), а будет ждать следующих клиентов. */
    int dummy = open(SERVER_FIFO, O_WRONLY);
    if (dummy == -1) { perror("open server fifo (w)"); return 1; }

    int received = 0;
    while (!stop && (limit < 0 || received < limit)) {
        struct request rq;
        ssize_t n = read(rfd, &rq, sizeof rq);
        int saved_errno = errno;              /* reap() может изменить errno */
        if (got_chld) { got_chld = 0; reap(WNOHANG); }
        if (n == -1) {
            errno = saved_errno;
            if (errno == EINTR) continue;
            perror("read request"); break;
        }
        if (n != (ssize_t)sizeof rq) { fprintf(stderr, "[server] неполный запрос (%zd байт)\n", n); continue; }
        rq.expr[EXPR_MAX - 1] = '\0';
        received++;

        pid_t pid = fork();
        if (pid == -1) { perror("fork"); continue; }
        if (pid == 0) {
            close(rfd); close(dummy);
            _exit(handle(&rq, delay_ms));      /* _exit: не сбрасывать буферы родителя */
        }
        active++;
        printf("[server %d] запрос #%d от клиента %d: \"%s\" -> обработчик PID=%d\n",
               (int)getpid(), received, (int)rq.client_pid, rq.expr, (int)pid);
    }

    printf("[server %d] завершение: жду %d активных обработчиков...\n", (int)getpid(), active);
    while (active > 0) {
        int status;
        pid_t pid = waitpid(-1, &status, 0);
        if (pid == -1) { if (errno == EINTR) continue; if (errno == ECHILD) break; perror("waitpid"); break; }
        active--;
        printf("[server %d] обработчик PID=%d завершился, код=%d\n", (int)getpid(), (int)pid,
               WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status));
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) served++; else failed++;
    }
    close(rfd); close(dummy);
    if (unlink(SERVER_FIFO) == -1) perror("unlink");
    printf("[server %d] итог: принято %d, успешно %d, с ошибкой %d. FIFO удалён.\n",
           (int)getpid(), received, served, failed);
    return 0;
}
