
void syscall_print(char *buf) {
	unsigned int ret;
	asm volatile("mov $0x0, %%eax; \
		mov %1, %%ebx; \
		int $0x80;"
		:"=r"(ret)
		:"r"(buf)
		: "%eax", "%ebx"
	);
}
void syscall_print2(char *buf) {
	unsigned int ret;
	asm volatile("mov $0x1, %%eax; \
		mov %1, %%ebx; \
		int $0x80;"
		:"=r"(ret)
		:"r"(buf)
		: "%eax", "%ebx"
	);
}
void syscall_ps() {
	unsigned int ret;
	asm volatile("mov $0x2, %%eax; \
		 \
		int $0x80;"
		:"=r"(ret)
		:
		: "%eax", "%ebx"
	);
}

int main()
{
	char buf[] = "We are in userland\n";
	syscall_print(buf);
	char buf2[] = "Horray all, now let's see some process list\n";
	syscall_print2(buf2);
	syscall_ps();
}
