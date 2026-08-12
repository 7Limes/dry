#include <stdio.h>
#include <string.h>
#include "globals.h"
#include "util/util.h"
#include "frontend/lexer.h"
#include "frontend/parser.h"
#include "stdlib/stdlib.h"
#include "compiler/compiler.h"


const char *DRY_VERSION = "0.1.0";


int compile_program(const char *infile, const char *outfile) {
    init_instruction_lookup();
    init_stdlib_lookup();

    char *program_string;
    size_t program_length;
    read_file_bytes((uint8_t**) &program_string, &program_length, infile);

    Lexer lexer = {0};
    int init_result = lexer_init(&lexer, program_string);
    if (init_result) {
        fprintf(stderr, "Error initializing lexer\n");
        return 1;
    }

    CtxProgram *program = parse_program(&lexer);
    if (program == NULL) {
        free(program_string);
        return 1;
    }

    CompilerContext *ctx = initialize_compiler();
    int got_error = 0;

    printf("Performing initial pass...\n");
    if (initial_pass(ctx, program)) goto error;

    printf("Performing filter pass...\n");
    if (filter_pass(ctx)) goto error;

    printf("Performing memory pass...\n");
    if (memory_pass(ctx)) goto error;

    printf("Performing codegen pass...\n");
    if (codegen_pass(ctx, outfile)) goto error;
    
    printf("Compilation completed.\n");
    goto cleanup;

    error:
    got_error = 1;
    
    cleanup:
    printf("Cleaning up...\n");
    cleanup_compiler(ctx);
    
    free(program_string);
    
    return got_error;
}


int main(int argc, char *argv[]) {
    if (argc == 1) {
        printf("usage: dry input_path output_path");
        return 0;
    }

    if (argc == 2) {
        if (strcmp(argv[1], "--version") == 0) {
            printf("dry compiler v%s\n", DRY_VERSION);
            return 0;
        }

        fprintf(stderr, "Expected output path");
        return 1;   
    }

    if (argc >= 3) {
        return compile_program(argv[1], argv[2]);
    }
}
