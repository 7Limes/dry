#ifndef _GLOBALS_H
#define _GLOBALS_H

#include "util/map.h"


extern const char *COL_ERROR;
extern const char *COL_RESET;


extern const char *TOKEN_KINDS[];

extern Map INSTRUCTION_LOOKUP;  // `Map[str, size_t]`


void init_instruction_lookup();

void set_default_constants(Map *constants);


#endif
