#include "../util/map.h"

#include "color.h"
#include "graphics.h"

Map STDLIB_LOOKUP = {0};


void init_stdlib_lookup() {
    Map *m = &STDLIB_LOOKUP;
    map_create(m, 64);

    map_add(m, ".color", stdlib_color);
    map_add(m, ".graphics", stdlib_graphics);
}
