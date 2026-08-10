#ifndef _COMPILER_H
#define _COMPILER_H

#include "frontend/parser.h"
#include "util/map.h"
#include "util/da.h"


typedef struct {
    size_t width, height;
    size_t current_address;
    Map constants;    // Map[str, int32_t]                 Maps constant names -> constant values
    Map globals;      // Map[str, VarData*]                Maps global names -> global addresses
    DynamicArray load_statements;  // DynamicArray[CtxLoadStatement]
    Map locals;       // Map[str, Map[str, VarData*]]      Maps proc names to a map of var name -> var data
    Map proc_params;  // Map[str, DynamicArray[size_t]]    Maps proc names -> list of its parameter addresses
    Map kept_names;   // Map[str, NULL]                    A set of names that will not be replaced with int values 

    DynamicArray procedures;     // DynamicArray[CtxProcedure*]
    DynamicArray program_nodes;  // DynamicArray[CtxProgram*]

    Map included_files;  // DynamicArray[str, NULL]

    int start_proc_exists, tick_proc_exists;
    size_t current_lexer_id;

} CompilerContext;


CompilerContext *initialize_compiler();
int initial_pass(CompilerContext *ctx, CtxProgram *program);
int filter_pass(CompilerContext *ctx);
int memory_pass(CompilerContext *ctx);
int codegen_pass(CompilerContext *ctx, const char *outfile);
int cleanup_compiler(CompilerContext *ctx);


#endif
