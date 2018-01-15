/*
 * Few functions to clean a path
 *  resolving multiple / or ./ or ../
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "console.h"
#include "serial.h"

/**
 * Cleans and solves multiple /, ./, ../, /.$ /..$
 *  expects first character to be '/'
 *  if last character is '/' and last is not first, trim it
 *  returns reference to path on success or NULL on failure 
 */
static char *clean_path(char *path)
{
    int i, j, len;
    len = strlen(path);
    for (i = 0, j = 0; i < len && j < len; i++) {
        if (path[i] == '/') {
            while (i < len && path[i] == '/')
                i++;
            if (i < len && path[i] == '.') {
                if (i + 1 == len || path[i + 1] == '/') {
                    continue;
                }
                if (path[i + 1] == '.') {
                    if (i + 2 == len || path[i + 2] == '/') {
                        i += 2;
                        if (j > 1 && path[j] == '/') j--;
                        while (j > 1 && path[j - 1] != '/')
                            j--;
                        if (i + 1 == len && j > 1 && path[j - 1] == '/')
                            j--;
                        continue;
                    }
                }
            }
            assert(j <= i);
            if (i < len || j == 0)
                path[j++] = '/';
            if (j == len)
                break;
        }
        assert(j <= i);
        path[j++] = path[i];
    }
    if (j == 0)
        j++;
    path[j] = 0;
    if (j > 1 && path[j - 1] == '/')
        j--;
    path[j] = 0;
    assert(j <= len);
    assert(j > 0);

    return path;
}

/**
 * Concatenates the prefix with the path and cleans it.
 * Using this in cofs path handling, prefix being process current working 
 * directory. 
 * If path is relative, we will construct it like prefix/path.
 * Returns a new allocated string. It's the caller responsability to free it
 */
char *canonize_path(char *prefix, char *path)
{
    char *p;
    if (path[0] == '/') {
        p = strdup(path);
    } else {
        if (prefix[0] != '/') {
            panic("canonize_path: prefix should start with /\n");
            return NULL;
        }
        p = malloc(strlen(prefix) + strlen(path) + 2);
        strcpy(p, prefix);
        strcat(p, "/");
        strcat(p, path);
    }
    if (!(clean_path(p))) {
        printf("Cannot clean path: %s\n", p);
        return NULL;
    }

    return p;
}


