#ifndef _CANONIZE_H
#define _CANONIZE_H

/**
 * Giving a prefix and path, constructs a new clean full path name.
 * Expects prefix to start with a '/'
 * Returns a new allocated string with path on success, NULL on failure.
 */
char *canonize_path(char *prefix, char *path);

#endif
