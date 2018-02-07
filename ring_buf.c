#include <stdlib.h>
#include "console.h"
#include "ring_buf.h"

ring_buf_t *rb_new(uint32_t size)
{
    ring_buf_t *rb = NULL;

    if (!(rb = malloc(sizeof(*rb)))) {
        kprintf("rb_new: out of memory\n");
        return NULL;
    }
    rb->buf = malloc(size);
    if (!rb->buf) {
        kprintf("rb_new: out of memory\n");
        free(rb);
        return NULL;
    }
    rb->head = rb->tail = 0;
    rb->size = size;

    return rb;
}

void rb_delete(ring_buf_t *rb)
{
    free(rb->buf);
    free(rb);
}

bool rb_is_empty(ring_buf_t *rb)
{
    return rb->head == rb->tail;
}

bool rb_is_full(ring_buf_t *rb)
{
    return rb->head - rb->tail >= rb->size;
}

void rb_putc(ring_buf_t *rb, char c)
{
    rb->buf[rb->head++ % rb->size] = c;
}

char rb_getc(ring_buf_t *rb)
{
    return rb->buf[rb->tail++ % rb->size];
}

void rb_reset(ring_buf_t *rb)
{
    rb->tail = rb->head = 0;
}

uint32_t rb_num_chars(ring_buf_t *rb)
{
    return rb->head - rb->tail;
}
