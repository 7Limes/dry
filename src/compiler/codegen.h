#ifndef _CODEGEN_H
#define _CODEGEN_H

#include <stdio.h>
#include "../frontend/lexer.h"

typedef struct {
    FILE *stream;
    size_t indent_level;
} CodeGenerator;


void create_code_generator(CodeGenerator *dest, const char *file_path);
void close_code_generator(const CodeGenerator *gen);

void emit(CodeGenerator *gen, const char *fmt, ...);
void emit_indent(CodeGenerator *gen);

void emit_token_value(CodeGenerator *gen, Token token);

#endif
