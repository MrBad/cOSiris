void irq_install(void);
void irq_install_handler(int irq, void (*handler)(struct iregs *r));
void irq_uninstall_handler(int irq);