#include "../util/map.h"

#include "color.h"
#include "font.h"
#include "graphics.h"
#include "math.h"
#include "string.h"

Map STDLIB_LOOKUP = {0};


void init_stdlib_lookup() {
    Map *m = &STDLIB_LOOKUP;
    map_create(m, 64);

    map_add(m, ".color", stdlib_color);
    map_add(m, ".font", stdlib_font);
    map_add(m, ".graphics", stdlib_graphics);
    map_add(m, ".math", stdlib_math);
    map_add(m, ".string", stdlib_string);
}
