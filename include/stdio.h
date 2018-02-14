#ifndef _STDIO_H
#define _STDIO_H

#include <sys/types.h>
#include <stdarg.h>

#ifndef NULL
#define NULL 0
#endif

#define EOF -1
#define OPEN_MAX 10

typedef struct _iobuf {
    char *buf;
    int head;
    int tail;
    int size;
    int flags;
    int fd;
} FILE;

struct _iob_head {
    FILE *files; // array of files for this process
    int slots;   // number of alocated slots < OPEN_MAX
};

extern struct _iob_head *_iob;

#define IOB_EMPTY(iob) (iob->tail == iob->head)
#define IOB_FULL(iob) ((iob->head - iob->tail) >= iob->size)

#define stdin  (&_iob->files[0])
#define stdout (&_iob->files[1])
#define stderr (&_iob->files[2])

FILE *fopen(const char *path, const char *mode);
int fclose(FILE *fp);
int feof(FILE *fp);
int fgetc(FILE *fp);
int fputc(int c, FILE *fp);
int getc(FILE *fp);
int putc(int c, FILE *fp);
int getchar();
int putchar(int c);
char *fgets(char *s, int size, FILE *fp);
int fputs(char *s, FILE *fp);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *fp);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *fp);
ssize_t getline(char **lineptr, size_t *n, FILE *fp);
int fflush(FILE *fp);

int vsprintf(char *str, const char *fmt, va_list ap);
int vsnprintf(char *str, size_t size, const char  *fmt,  va_list ap);
int sprintf(char *buf, const char *fmt, ...);
int snprintf(char *str, size_t size, const char *fmt, ...);
int printf(char *fmt, ...);
int fprintf(FILE *fp, const char *fmt, ...);

int vsscanf(const char *str, const char *fmt, va_list ap);
int sscanf(const char *str, const char *fmt, ...);

void perror(const char *s);

#endif
