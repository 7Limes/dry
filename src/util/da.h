#ifndef _DA_H
#define _DA_H

#include <stddef.h>

#define DA_DEFAULT_CAPACITY 4

typedef struct {
    void **data;
    size_t length, capacity;
} DynamicArray;


int da_append(DynamicArray *arr, void *value);
int da_pop(DynamicArray *arr, void **dest);
void da_free(const DynamicArray *arr);
void da_free_all(const DynamicArray *arr);


#define da_free_all_with(arr, free_func) do { \
    if (arr.capacity != 0) { \
        for (size_t i = 0; i < arr.length; i++) { \
            free_func(arr.data[i]); \
        } \
        free(arr.data); \
    } \
} while (0) \


#endif
