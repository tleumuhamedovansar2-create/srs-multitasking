/* common.h — общий протокол клиента и сервера на FIFO. СРС 1, задание 2, вариант 8. */
#ifndef COMMON_H
#define COMMON_H

#include <limits.h>
#include <sys/types.h>

#define SERVER_FIFO   "/tmp/srs8_server.fifo"          /* общий FIFO запросов   */
#define CLIENT_FIFO_FMT "/tmp/srs8_client.%d.fifo"     /* личный FIFO клиента   */
#define EXPR_MAX      200
#define ANSWER_MAX    256

/* Запрос фиксированного размера. sizeof(struct request) <= PIPE_BUF (4096),
 * поэтому запись в FIFO атомарна: запросы разных клиентов не перемешиваются. */
struct request {
    pid_t client_pid;
    char  expr[EXPR_MAX];
};

struct answer {
    int    ok;                 /* 1 — вычислено, 0 — ошибка в выражении */
    double value;
    pid_t  worker_pid;         /* PID процесса сервера, обработавшего запрос */
    char   text[ANSWER_MAX];   /* текст результата или сообщения об ошибке */
};

_Static_assert(sizeof(struct request) <= PIPE_BUF, "request must fit in PIPE_BUF");

#endif
