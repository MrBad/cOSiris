#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

#define BUF_SIZE 512
#define GETLINE_INIT_SIZE 16
#define IOB_INIT_NUMSLOTS 16

enum file_flags {
    F_RDONLY = 00000001,
    F_WRONLY = 00000002,
    F_RDWR  =  00000004,
    F_UNBUF =  00000010,
    F_EOF =    00000020,
    F_ERR =    00000040,
};

static char *_alloc_buf(FILE *fp, int size)
{
    int bsize = fp->flags & F_UNBUF && size > 0 ? 1 : size;
    fp->buf = realloc(fp->buf, bsize);
    if (!fp->buf)
        return NULL;
    fp->size = bsize;
    return fp->buf;
}

/**
 * This will close all open files and destroy the iob.
 * Call this when process exits, using something like atexit
 */
static void _iob_destroy()
{
    int i;
    if (!_iob)
        return;

    // Close still open files. fclose will flush their buffers.
    for (i = 0; i < _iob->slots; i++)
        if (_iob->files[i].flags > 0)
            fclose(_iob->files + i);

    free(_iob->files);
    free(_iob);
    _iob = NULL;
}

/**
 * Sets up _iob internals, stdin, stdout and stderr
 */
int _iob_init()
{
    FILE *fp;
    assert(IOB_INIT_NUMSLOTS > 2);
    if (_iob != NULL)
        return -1;
    if (!(_iob = malloc(sizeof(*_iob))))
        return -1;

    _iob->files = calloc(IOB_INIT_NUMSLOTS, sizeof(*_iob->files));
    if (!_iob->files)
        return -1;
    _iob->slots = IOB_INIT_NUMSLOTS;
    fp = _iob->files;
    fp->fd = STDIN_FILENO;
    fp->flags = F_RDONLY;
    fp->buf = _alloc_buf(fp, BUF_SIZE);
    fp++;
    fp->fd = STDOUT_FILENO;
    fp->flags = F_WRONLY;
    fp->buf = _alloc_buf(fp, BUF_SIZE);
    fp++;
    fp->fd = STDERR_FILENO;
    fp->flags = F_WRONLY | F_UNBUF;
    fp->buf = _alloc_buf(fp, 1);

    atexit(_iob_destroy);
    return 0;
}

/**
 * Allocates a FILE structure
 * returns a pointer to the allocated file or NULL on error
 */
FILE *_alloc_file()
{
    FILE *fp = NULL, *new_fb;
    // search for a free slot
    for (fp = _iob->files; fp < _iob->files + _iob->slots; fp++)
        if (fp->flags == 0)
            break;
    // If we don't have a free slot, expand files buf if OPEN_MAX not reached.
    if (fp == _iob->files + _iob->slots) {
        if (_iob->slots >= OPEN_MAX)
            return NULL;

        new_fb = realloc(_iob->files, 2 * _iob->slots * sizeof(*_iob->files));

        if (new_fb) {
            _iob->files = new_fb;
            fp = _iob->files + _iob->slots;
            memset(fp, 0, sizeof(*_iob->files) * _iob->slots);
            _iob->slots *= 2;
        } else {
            return NULL;
        }
    }

    return fp;
}

FILE *fopen(const char *path, const char *mode)
{
    FILE *fp;
    int flags = 0;

    if (*mode != 'r' && *mode != 'w' && *mode != 'a')
        return NULL;

    if ((fp = _alloc_file()) == NULL)
        return NULL;

    if (*mode == 'w')
        flags = O_CREAT | O_TRUNC;
    else if (*mode == 'a')
        flags = O_CREAT | O_APPEND;

    if (mode[1] == '+')
        flags |= O_RDWR;
    else
        flags |= *mode == 'r' ? O_RDONLY : O_WRONLY;

    if ((fp->fd = open(path, flags, *mode == 'r' ? 0 : 0666)) < 0)
        return NULL;
    
    fp->flags = 0;
    if (flags & O_RDONLY)
        fp->flags |= F_RDONLY;
    else if (flags & O_WRONLY)
        fp->flags |= F_WRONLY;
    else 
        fp->flags |= F_RDWR;

    fp->buf = _alloc_buf(fp, BUF_SIZE);
    fp->head = fp->tail = 0;

    return fp;
}

static int _fill_buf(FILE *fp)
{
    int num_bytes, rel_pos;
    
    if (fp->flags & (F_EOF | F_ERR | F_WRONLY))
        return EOF;

    assert(fp->buf);

    rel_pos = fp->head % fp->size;
    num_bytes = read(fp->fd, fp->buf + rel_pos, fp->size - rel_pos);

    if (num_bytes <= 0) {
        if (num_bytes == 0)
            fp->flags |= F_EOF;
        else if (num_bytes < 0)
             fp->flags |= F_ERR;
        return EOF;
    }

    fp->head += num_bytes;
    return num_bytes;
}

static int _flush_buf(FILE *fp)
{
    int num_bytes, rel_pos, size;

    if (fp->flags & (F_EOF | F_ERR | F_RDONLY))
        return EOF;

    assert(fp->buf);

    size = fp->head - fp->tail;
    if (size && (fp->flags & F_UNBUF))
        size = 1;
    rel_pos = fp->tail % fp->size;
    num_bytes = write(fp->fd, fp->buf + rel_pos, size);
    if (num_bytes <= 0) {
        fp->flags |= F_ERR;
        return EOF;
    }
    fp->tail += num_bytes;
    return num_bytes;
}

int feof(FILE *fp)
{
    return fp->flags & F_EOF;
}

int fgetc(FILE *fp)
{
    if (IOB_EMPTY(fp))
        if (_fill_buf(fp) < 0)
            return EOF;

    return (int) fp->buf[fp->tail++ % fp->size];
}

int fputc(int c, FILE *fp)
{
    fp->buf[fp->head++ % fp->size] = (char) c;
 
    if (IOB_FULL(fp) || (fp->flags & F_UNBUF))
        if (_flush_buf(fp) < 0)
            return EOF;

    return c;
}

int getc(FILE *fp) { return fgetc(fp); }
int getchar() { return getc(stdin); }
int putc(int c, FILE *fp) { return fputc(c, fp); }
int putchar(int c) { return putc(c, stdout); }

char *fgets(char *s, int size, FILE *fp)
{
    char c;
    int i = 0;

    while (((c = getc(fp)) != EOF) && (i < size - 1)) {
        s[i++] = c;
        if (c == '\n')
            break;
    }
    s[i] = 0;

    return i == 0 ? NULL : s;
}

ssize_t getline(char **lineptr, size_t *n, FILE *fp)
{
    size_t i = 0;
    char c, *s = *lineptr;

    for (;;) {
        if (*lineptr == NULL || i + 1 >= *n) {
            *n = *n == 0 ? GETLINE_INIT_SIZE : *n * 2;
            s = *lineptr = realloc(*lineptr, *n);
        }
        if ((c = getc(fp)) == EOF)
            break;
        s[i++] = c;
        if (c == '\n')
            break;
    }
    s[i] = 0;

    if (fp->flags & (F_EOF | F_ERR))
        return EOF;
    return i;
}

int fputs(char *s, FILE *fp)
{
    while(*s) {
        if (fputc(*s++, fp) < 0)
            return EOF;
    }
    return *s;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *fp)
{
    char *p = ptr;
    size_t i = 0, j = 0;
    while (i < nmemb * size) {
       if(IOB_EMPTY(fp))
           if (_fill_buf(fp) < 0)
               break;
       p[i++] = fp->buf[fp->tail++ % fp->size];
       if ((i % size) == 0)
           j++;
    }

    return j;
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *fp)
{
    const char *p = ptr;
    size_t i = 0, j = 0;
    while (i < nmemb * size) {
       fp->buf[fp->head++ % fp->size] = p[i++];
       if(IOB_FULL(fp))
           if (_flush_buf(fp) < 0)
               break;
       if ((i % size) == 0)
           j++;
    }

    return j;
}

int fflush(FILE *fp) 
{
    int ret = 0;

    if (fp) {
        if (fp->flags & O_RDONLY) {
            fp->tail = fp->head;
        } else {
            if (_flush_buf(fp) < 0)
                ret = EOF;
        }
    } else {
        for (fp = _iob->files; fp < _iob->files + _iob->slots; fp++) {
            if ((fp->flags & (F_RDWR | F_WRONLY))
                    && fflush(fp) == -1)
                ret = -1;
        }
    }

    return ret;
}

int fclose(FILE *fp)
{
    int ret;
    fflush(fp);
    ret = close(fp->fd);
    free(fp->buf);
    memset(fp, 0, sizeof(*fp));

    return ret;
}

