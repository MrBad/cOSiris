#ifndef _RING_BUF
#define _RING_BUF

typedef struct ring_buf {
    char *buf;
    uint32_t head;
    uint32_t tail;
    uint32_t size;
} ring_buf_t;

ring_buf_t *rb_new(uint32_t size);
void rb_delete(ring_buf_t *rb);
bool rb_is_empty(ring_buf_t *rb);
bool rb_is_full(ring_buf_t *rb);
void rb_putc(ring_buf_t *rb, char c);
char rb_getc(ring_buf_t *rb);
void rb_reset(ring_buf_t *rb);
uint32_t rb_num_chars(ring_buf_t *rb);

#endif

