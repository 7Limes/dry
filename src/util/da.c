#include <stdlib.h>
#include "da.h"


int da_append(DynamicArray *arr, void *value) {
    if (arr->capacity == 0) {
        arr->capacity = DA_DEFAULT_CAPACITY;
        arr->data = calloc(sizeof(void*), DA_DEFAULT_CAPACITY);
    }

    arr->data[arr->length] = value;

    arr->length++;
    if (arr->length >= arr->capacity) {
        size_t new_size = sizeof(void*) * arr->capacity*2;
        void **new_data = realloc(arr->data, new_size);
        if (!new_data) {
            return 1;
        }
        arr->data = new_data;
        arr->capacity *= 2;
    }

    return 0;
}

void da_free(const DynamicArray *arr) {
    if (arr->capacity != 0) {
        free(arr->data);
    }
}

void da_free_all(const DynamicArray *arr) {
    if (arr->capacity == 0) {
        return;
    }

    for (size_t i = 0; i < arr->length; i++) {
        free(arr->data[i]);
    }
    free(arr->data);
}
