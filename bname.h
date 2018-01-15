#ifndef _BNAME_H
#define _BNAME_H
#define MAX_PATH_LEN 512

/**
 * Giving a path name, split it into directory name and base name
 */
void bname(char *path, char *dirname, char *basename);

#endif
