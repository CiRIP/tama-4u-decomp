/* The toolchain's library code linked into the firmware (config/objects.txt kind lib, and
   src/lib/sys.c): GNU33 lib/pe libc.a, libgcc.a and libstdio.a. Addresses in config/symbols.txt. */
#ifndef LIBC_H
#define LIBC_H

#include <libstdio.h>       /* _init_lib (libstdio lib.o), _init_sys (src/lib/sys.c) */

typedef unsigned int size_t;

/* libc.a */
void *malloc(size_t);
int ansi_InitMalloc(unsigned long, unsigned long);     /* heap area start, end */
int rand(void);
void srand(unsigned int);
char *strcat(char *, const char *);
size_t strlen(const char *);
int strncmp(const char *, const char *, size_t);
char *strncpy(char *, const char *, size_t);
char *strcpy(char *, const char *);
void *memcpy(void *, const void *, size_t);
int memcmp(const void *, const void *, size_t);
void *memset(void *, int, size_t);

/* src/lib/sys.c */
void read_str(char *);
void write_str(char *);
int read(int, char *, int);
int write(int, char *, int);

#endif
