/* calc.c — разбор арифметического выражения методом рекурсивного спуска.
 * expr   := term   { ('+'|'-') term }
 * term   := factor { ('*'|'/') factor }
 * factor := number | '(' expr ')' | '-' factor | '+' factor            */
#include "calc.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

struct parser { const char *p; const char *err; };

static void skip(struct parser *ps) { while (isspace((unsigned char)*ps->p)) ps->p++; }
static double expr(struct parser *ps);

static double factor(struct parser *ps)
{
    skip(ps);
    if (ps->err) return 0;
    if (*ps->p == '(') {
        ps->p++;
        double v = expr(ps);
        skip(ps);
        if (*ps->p != ')') { if (!ps->err) ps->err = "ожидалась ')'"; return 0; }
        ps->p++;
        return v;
    }
    if (*ps->p == '-') { ps->p++; return -factor(ps); }
    if (*ps->p == '+') { ps->p++; return  factor(ps); }
    char *end;
    double v = strtod(ps->p, &end);
    if (end == ps->p) { ps->err = "ожидалось число"; return 0; }
    ps->p = end;
    return v;
}

static double term(struct parser *ps)
{
    double v = factor(ps);
    for (;;) {
        skip(ps);
        if (ps->err) return 0;
        char op = *ps->p;
        if (op != '*' && op != '/') return v;
        ps->p++;
        double r = factor(ps);
        if (op == '*') v *= r;
        else {
            if (r == 0) { ps->err = "деление на ноль"; return 0; }
            v /= r;
        }
    }
}

static double expr(struct parser *ps)
{
    double v = term(ps);
    for (;;) {
        skip(ps);
        if (ps->err) return 0;
        char op = *ps->p;
        if (op != '+' && op != '-') return v;
        ps->p++;
        double r = term(ps);
        v = (op == '+') ? v + r : v - r;
    }
}

int calc_eval(const char *s, double *result, char *err, int errsz)
{
    struct parser ps = { s, NULL };
    double v = expr(&ps);
    skip(&ps);
    if (!ps.err && *ps.p != '\0') ps.err = "лишние символы в конце";
    if (ps.err) { snprintf(err, errsz, "ошибка: %s (позиция %d)", ps.err, (int)(ps.p - s)); return -1; }
    *result = v;
    return 0;
}
