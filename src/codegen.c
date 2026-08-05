#include <stdio.h>
#include <stdarg.h>
#include "frontend/lexer.h"
#include "codegen.h"


void create_code_generator(CodeGenerator *dest, const char *file_path) {
    fclose(fopen(file_path, "w"));  // Clear file
    
    dest->stream = fopen(file_path, "a");
    dest->indent_level = 0;
}


void close_code_generator(const CodeGenerator *gen) {
    fclose(gen->stream);
}


void emit(CodeGenerator *gen, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(gen->stream, fmt, args);
    va_end(args);
}


void emit_indent(CodeGenerator *gen) {
    for (size_t i = 0; i < gen->indent_level; i++) {
        fprintf(gen->stream, "    ");
    }
}


void emit_token_value(CodeGenerator *gen, Token token) {
    const char *str = token.source + token.index;

    for (size_t i = 0; i < token.length; i++) {
        fprintf(gen->stream, "%c", str[i]);
    }
}
