void * memmove(void *mdest, void *msrc, int n)
{
	char *dst, *src;
	dst = mdest;
	src = msrc;
	while(n-- > 0) {
		*dst++ = *src++;
	}
	return mdest;
}
