#ifndef _TYPES_H
#define _TYPES_H

typedef unsigned long ulong_t;
#define NULL (void *) 0
#define false 0
#define true 1
typedef unsigned char bool;
typedef unsigned int size_t;

#ifndef _uint32_t
#define _uint32_t
typedef unsigned int uint32_t;
#endif

typedef int int32_t;
typedef unsigned short int uint16_t;
typedef short int int16_t;
typedef unsigned char uint8_t;
typedef char int8_t;

typedef int pid_t;
typedef unsigned int off_t;
typedef unsigned int time_t;
typedef unsigned short uid_t;
typedef unsigned char gid_t;
typedef unsigned short dev_t;
typedef unsigned short ino_t;
typedef unsigned short mode_t;
typedef unsigned short umode_t;
typedef unsigned char nlink_t;


#endif
