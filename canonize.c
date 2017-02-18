#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//
//	Few functions to clean a path
//		resolving multiple / or ./ or ../
//

// cleans and solves multiple /, ./, ../, /.$ /..$
// expects first character to be '/'
// if last character is '/' and last is not first, trim it
// returns reference to path on success or NULL on failure 
static char *clean_path(char *path) 
{
//	printf("cleaning: %s\n", path);
	char *p, *k;
	k = p = path;
	while(*p) {
		if(*p=='/'){
			while(*p=='/') p++;
			if(*p == '.') {
				p++;
				if(*p=='/' || *p==0) { // match /./ or /.$
					continue;
				} else if(*p=='.') {
					p++;
					if(*p=='/' || *p==0) { // match /../ or /..$
						if(*k=='/' && k > path) k--;
						while(*k!='/') {
							if(k-1 < path)	// if k points t begining of path
								break;		// fail
											// should we fail or just discard 
											// ../ ?
							*k-- = 0;
						}
						continue;
					}
					p--;
				}
				p--;
			}
			*k++='/';
			continue;
		}
		*k++ = *p++;
	}
	*k=0;
	if(k < path) {
		printf("ERRRRRRRRRRR\n");
	}
	// trim last /, only if it's not first :D //
	if(k > path+1) {
		if(*(k-1)=='/') {
			k--;
			*k = 0;
		}
	}
	if(k==path) {
		 *k++='/';*k=0;
	}

	//printf("cleaned to: %s, len: %ld, strlen: %ld\n", path, k-path, strlen(path));
	return path;
}

//
//	giving a prefix and path, if path is relative constructs a string 
//		and cleans it
//	I will use this in cOSiris, prefix being process current working directory
//	and path - the path. If path is relative, we will construct it like
//	prefix/path
//	Expects prefix to start with /
//	return a new string with path cleaned on success, NULL on failure. 
//	It's the caller responsability to free it
//
char *canonize_path(char *prefix, char *path) 
{
	char *p;
	int len;
	if(path[0] == '/') {
		len = strlen(path)+10;
		p = malloc(len);
		strcpy(p, path);
		p[len] = 0;
	} else {
		if(prefix[0]!='/') {
			printf("Error - prefix should start with /\n");
			return NULL;
		}
		len = strlen(prefix)+strlen(path)+20;
		p = malloc(len);
		strcpy(p, prefix);
		strcat(p, "/");
		strcat(p, path);
	}
	if(!(clean_path(p))){
		printf("Cannot clean path: %s\n", p);
		return NULL;
	}
//	printf("[%s]\n",p);
	return p;
}


