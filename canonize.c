#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "serial.h"
//
//	Few functions to clean a path
//		resolving multiple / or ./ or ../
//

// cleans and solves multiple /, ./, ../, /.$ /..$
// expects first character to be '/'
// if last character is '/' and last is not first, trim it
// returns reference to path on success or NULL on failure 
char *clean_path(char *path) 
{
//	printf("path: %s\n",path);
	int i,j,len;
	len=strlen(path);
	for(i = 0, j = 0; i < len && j < len; i++) {
		if(path[i]=='/') {
			while(i < len && path[i]=='/')
				i++;
			if(i < len && path[i]=='.') {
				if(i+1==len || path[i+1]=='/') {
					continue;
				}
				if(path[i+1]=='.') {
					if(i+2==len || path[i+2]=='/') {
						i+=2;
						if(j > 1 && path[j]=='/') j--;
						while(j > 1 && path[j-1]!='/') j--;
						if(i+1==len && j > 1 && path[j-1]=='/') j--;
						continue;
					}
				}
			}
			assert(j <= i);
			if(i<len || j==0) path[j++]='/';
			if(j==len) break;
		}
		assert(j <= i);
		path[j++]=path[i];
	}
	if(j==0)j++;
	path[j]=0;
	if(j > 1 && path[j-1]=='/') j--;path[j]=0;
	assert(j<=len);
	assert(j>0);
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
	serial_debug("canonize_path: prefix: [%s] path: [%s] ", prefix, path);
	if(path[0]=='/') {
		p = strdup(path);
	} else {
		if(prefix[0]!='/') {
			panic("canonize_path: prefix should start with /\n");
			return NULL;
		}
		p = malloc(strlen(prefix)+strlen(path)+2);
		strcpy(p, prefix);
		strcat(p, "/");
		strcat(p, path);
	}
	
	if(!(clean_path(p))){
		printf("Cannot clean path: %s\n", p);
		return NULL;
	}
	serial_debug(" to [%s]\n",p);
	return p;
}


