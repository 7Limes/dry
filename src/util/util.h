#ifndef _UTIL_H
#define _UTIL_H

#include <stdint.h>
#include <stddef.h>


#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

int file_exists(char* path);
int read_file_bytes(uint8_t **output_buffer, size_t *length, const char *file_path);
int str_to_int(const char *str, int *out);


#endif
