#ifndef __MAP_H
#define __MAP_H

#include <stdlib.h>
#include <stdint.h>


typedef struct {
    char *key;
    void *value;
} MapNode;


typedef struct {
    size_t size, capacity;
    MapNode *data;
} Map;


int map_create(Map *map_dest, size_t capacity);

void map_free(const Map *map);

void map_free_all(const Map *map);

int map_add(Map *map, const char *key, void *value);

int map_get(void **dest, Map *map, const char *key);


#define map_free_all_with(map, free_func) do { \
    for (size_t i = 0; i < map.capacity; i++) { \
        MapNode *node = &map.data[i]; \
        if (node->key != NULL) { \
            free_func(node->value); \
        } \
    } \
    map_free(&map); \
} while(0)


#endif