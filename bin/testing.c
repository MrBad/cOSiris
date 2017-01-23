
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

int main()
{
	char buf[] = "Testing this thing\n";
	syscall_print(buf);
}
