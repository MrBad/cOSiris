void * memmove(void *dst, void *src, int n)
{
    char *d, *s;

    if (dst < src) {
        d = dst;
        s = src;
        while (n-- > 0)
            *d++ = *s++;
    } else {
        d = dst + n;
        s = src + n;
        while (n-- > 0)
            *--d = *--s;
    }

    return dst;
}
