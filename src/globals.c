#include "util/map.h"

const char *COL_ERROR = "\x1b[31m";
const char *COL_RESET = "\x1b[0m";


const char *TOKEN_KINDS[] = {
    "LOAD",
    "DEFINE",
    "INCLUDE",
    "LPAREN",
    "RPAREN",
    "LBRACE",
    "RBRACE",
    "LBRACKET",
    "RBRACKET",
    "COMMA",
    "DOLLAR_SIGN",
    "HASHTAG",
    "NAME",
    "INT",
    "STRING"
};


Map INSTRUCTION_LOOKUP = {0};


int map_add_int(Map *map, const char *key, size_t value) {
    map_add(map, key, (void*) value);
}


void init_instruction_lookup() {
    Map *m = &INSTRUCTION_LOOKUP;
    map_create(m, 64);

    map_add_int(m, "ldi", 2);
    map_add_int(m, "rdi", 2);
    map_add_int(m, "sti", 2);
    map_add_int(m, "add", 3);
    map_add_int(m, "mul", 3);
    map_add_int(m, "div", 3);
    map_add_int(m, "mod", 3);
    map_add_int(m, "cmp", 3);
    map_add_int(m, "jne", 3);
    map_add_int(m, "col", 3);
    map_add_int(m, "pix", 2);

    map_add_int(m, "cpy", 2);
    map_add_int(m, "sub", 3);
    map_add_int(m, "addi", 3);
    map_add_int(m, "subi", 3);
    map_add_int(m, "muli", 3);
    map_add_int(m, "divi", 3);
    map_add_int(m, "modi", 3);
    map_add_int(m, "inc", 1);
    map_add_int(m, "dec", 1);
    map_add_int(m, "abs", 2);
    map_add_int(m, "jnea", 3);
    map_add_int(m, "ja", 1);
    map_add_int(m, "sne", 2);
    map_add_int(m, "ret", 0);
    
    map_add_int(m, "call", 1);
    map_add_int(m, "brkeq", 2);
    map_add_int(m, "brkne", 2);
    map_add_int(m, "brkge", 2);
    map_add_int(m, "brkle", 2);
    map_add_int(m, "brk", 0);
}


void set_default_constants(Map *constants) {
    map_add_int(constants, "NEG1", 0x2);
    map_add_int(constants, "CONTROL1", 0x3);
    map_add_int(constants, "CONTROL2", 0x4);
    map_add_int(constants, "A", 0x5);
    map_add_int(constants, "B", 0x6);
    map_add_int(constants, "UP", 0x7);
    map_add_int(constants, "DOWN", 0x8);
    map_add_int(constants, "LEFT", 0x9);
    map_add_int(constants, "RIGHT", 0xa);
    map_add_int(constants, "DELTA", 0xb);
}
