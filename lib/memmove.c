void * memmove(void *dst, void *src, int n)
{
    char *d, *s;

    d = dst;
    s = src;
    if (dst < src) {
        while (n-- > 0)
            *d++ = *s++;
    } else {
        d += n;
        s += n;
        while (n-- > 0)
            *--d = *--s;
    }

    return dst;
}
