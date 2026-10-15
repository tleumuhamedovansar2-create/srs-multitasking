/* procfd.c — вывод всех открытых дескрипторов процесса и их назначения.
 * Источник: /proc/<pid>/fd (readlink) и /proc/<pid>/fdinfo (позиция, флаги).
 * СРС 1, задание 1, вариант 8.
 * Использование: ./procfd [PID]   (по умолчанию — сам процесс procfd)
 *                ./procfd -demo   (открыть файл, pipe и сокет, затем показать себя) */
#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *fd_kind(const char *target)
{
    if (strncmp(target, "pipe:", 5) == 0)        return "канал (pipe)";
    if (strncmp(target, "socket:", 7) == 0)      return "сокет";
    if (strncmp(target, "anon_inode:", 11) == 0) return "анонимный inode";
    if (strncmp(target, "/dev/pts/", 9) == 0 ||
        strncmp(target, "/dev/tty", 8) == 0)     return "терминал";
    if (strcmp(target, "/dev/null") == 0)        return "/dev/null";
    if (strncmp(target, "/dev/", 5) == 0)        return "устройство";
    if (target[0] == '/')                        return "файл/каталог";
    return "другое";
}

/* printf("%-Ns") считает байты, а не символы; кириллица в UTF-8 занимает 2 байта.
 * Печатаем строку и добиваем пробелами по числу символов. */
static void print_padded(const char *s, int width)
{
    int chars = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++)
        if ((*p & 0xC0) != 0x80) chars++;
    fputs(s, stdout);
    for (; chars < width; chars++) putchar(' ');
}

static const char *acc_mode(unsigned flags)
{
    switch (flags & O_ACCMODE) {
    case O_RDONLY: return "r-";
    case O_WRONLY: return "-w";
    case O_RDWR:   return "rw";
    default:       return "??";
    }
}

/* Прочитать pos и flags из /proc/<pid>/fdinfo/<fd> */
static void read_fdinfo(const char *pid, const char *fd, long long *pos, unsigned *flags)
{
    char path[PATH_MAX];
    snprintf(path, sizeof path, "/proc/%s/fdinfo/%s", pid, fd);
    *pos = -1; *flags = 0;
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof line, f)) {
        if (strncmp(line, "pos:", 4) == 0)   *pos = strtoll(line + 4, NULL, 10);
        if (strncmp(line, "flags:", 6) == 0) *flags = (unsigned)strtoul(line + 6, NULL, 8);
    }
    fclose(f);
}

static int show_fds(const char *pid)
{
    char dirpath[PATH_MAX], comm[64] = "?";
    snprintf(dirpath, sizeof dirpath, "/proc/%s/comm", pid);
    FILE *cf = fopen(dirpath, "r");
    if (cf) { if (fgets(comm, sizeof comm, cf)) comm[strcspn(comm, "\n")] = 0; fclose(cf); }

    snprintf(dirpath, sizeof dirpath, "/proc/%s/fd", pid);
    DIR *d = opendir(dirpath);
    if (!d) {
        fprintf(stderr, "opendir %s: %s%s\n", dirpath, strerror(errno),
                errno == EACCES ? " (нужны права владельца или root)" : "");
        return -1;
    }
    printf("Процесс %s (%s): открытые дескрипторы\n", pid, comm);
    printf("%-4s %-3s %-10s ", "FD", "RW", "POS");
    print_padded("ТИП", 16);
    printf(" КУДА УКАЗЫВАЕТ\n");
    printf("---- --- ---------- ---------------- ------------------------------\n");

    int count = 0;
    struct dirent *e;
    errno = 0;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        char link[PATH_MAX + 300], target[PATH_MAX];
        snprintf(link, sizeof link, "%s/%s", dirpath, e->d_name);
        ssize_t n = readlink(link, target, sizeof target - 1);
        if (n == -1) {
            /* дескриптор самого opendir может закрыться — это не ошибка программы */
            if (errno == EACCES) {
                fprintf(stderr, "readlink %s: %s (нужны права владельца или root)\n", link, strerror(errno));
                closedir(d);
                return -1;
            }
            if (errno != ENOENT) fprintf(stderr, "readlink %s: %s\n", link, strerror(errno));
            errno = 0;
            continue;
        }
        target[n] = '\0';
        long long pos; unsigned flags;
        read_fdinfo(pid, e->d_name, &pos, &flags);
        char posbuf[24];
        if (pos >= 0) snprintf(posbuf, sizeof posbuf, "%lld", pos); else strcpy(posbuf, "-");
        printf("%-4s %-3s %-10s ", e->d_name, acc_mode(flags), posbuf);
        print_padded(fd_kind(target), 16);
        printf(" %s\n", target);
        count++;
        errno = 0;
    }
    if (errno != 0) perror("readdir");
    if (closedir(d) == -1) perror("closedir");
    printf("Всего дескрипторов: %d\n", count);
    return 0;
}

int main(int argc, char *argv[])
{
    char pid[32];
    if (argc > 1 && strcmp(argv[1], "-demo") == 0) {
        /* Откроем ресурсы разных типов, чтобы их было видно в таблице */
        int f = open("/etc/hostname", O_RDONLY);
        if (f == -1) perror("open /etc/hostname");
        int tmp = open("procfd_demo.txt", O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (tmp == -1) perror("open procfd_demo.txt");
        else if (write(tmp, "hello\n", 6) == -1) perror("write");
        int p[2];
        if (pipe(p) == -1) perror("pipe");
        int s = socket(AF_INET, SOCK_STREAM, 0);
        if (s == -1) perror("socket");
        snprintf(pid, sizeof pid, "%d", (int)getpid());
    } else if (argc > 1) {
        char *end;
        long v = strtol(argv[1], &end, 10);
        if (*end != '\0' || v <= 0) {
            fprintf(stderr, "Использование: %s [PID | -demo]\n", argv[0]);
            return EXIT_FAILURE;
        }
        snprintf(pid, sizeof pid, "%ld", v);
    } else {
        snprintf(pid, sizeof pid, "%d", (int)getpid());
    }
    return show_fds(pid) == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
