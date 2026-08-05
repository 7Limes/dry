#include <stdio.h>
#include <string.h>
#include "util/util.h"
#include "globals.h"
#include "frontend/lexer.h"
#include "frontend/parser.h"
#include "compiler.h"


int main(int argc, char *argv[]) {
    const char *INFILE = "test.dry";
    const char *OUTFILE = "output.g2";

    init_instruction_lookup();

    char *program_string;
    size_t program_length;
    read_file_bytes((uint8_t**) &program_string, &program_length, INFILE);

    Lexer lexer = {0};
    int init_result = lexer_init(&lexer, program_string);
    if (init_result) {
        fprintf(stderr, "Error initializing lexer\n");
        return 1;
    }

    CtxProgram *program = parse_program(&lexer);

    CompilerContext *ctx = initialize_compiler();

    printf("Performing initial pass...\n");
    if (initial_pass(ctx, program)) goto cleanup;
    printf("Performing memory pass...\n");
    if (memory_pass(ctx)) goto cleanup;
    printf("Performing codegen pass...\n");
    if (codegen_pass(ctx, OUTFILE)) goto cleanup;

    cleanup:
    printf("Cleaning up...\n");
    cleanup_compiler(ctx);
    
    free(program_string);
    
    return 0;
}
