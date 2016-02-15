unsigned int *dir, *table;
	dir = (unsigned int *) pop_frame();
	table = (unsigned int *) pop_frame();
	
	dir[0] = ((unsigned int) table | P_PRESENT | P_READ_WRITE);
	for (i = 0; i < 1024; i++) {
		table[i] = i * PAGE_SIZE | P_PRESENT | P_READ_WRITE;
	}
	debug_print_directory((page_directory_t *) dir);
	kprintf("%X, %X, %X, %X\n", dir, dir[0], table, table[1]);
	switch_page_directory(dir);

OR

unsigned int *dir, *table;
	dir = (unsigned int *) pop_frame();
	for (i = 0; i < 1024 * PAGE_SIZE; i+=PAGE_SIZE) {
		if(!dir[0]) {
			table = (unsigned int *) pop_frame();
			dir[0] = ((unsigned int) table | P_PRESENT | P_READ_WRITE);
			kprintf("Assigning table\n");
		}
		table[i/PAGE_SIZE] = i | P_PRESENT | P_READ_WRITE;
	}
	debug_print_directory((page_directory_t *) dir);
	kprintf("%X, %X, %X, %X\n", dir, dir[0], table, table[1]);
	switch_page_directory(dir);
	goto exit;
