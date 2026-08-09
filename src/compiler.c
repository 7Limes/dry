#include <stdio.h>
#include <string.h>
#include "util/map.h"
#include "util/da.h"
#include "util/util.h"
#include "frontend/parser.h"
#include "codegen.h"
#include "compiler.h"
#include "globals.h"

#define PSEUDO_ARITHMETIC_REGISTER 0xe
#define EXTRA_ARITHMETIC_REGISTER 0x20
#define FIRST_FRAME_FLAG_ADDRESS 0x21
#define START_ADDRESS 0x22

const char *START_PROC_NAME = "start";
const char *TICK_PROC_NAME = "tick";
const char *END_LABEL_NAME = "__end";

typedef struct {
    size_t address, size;
} VarData;


typedef struct {
    CompilerContext *ctx;
    CodeGenerator *gen;
    const char *proc_name;
    Map *proc_locals;
    DynamicArray *proc_params;
    const char *break_label;

} CodegenContext;


// Forward declarations
int initial_pass(CompilerContext *ctx, CtxProgram *program);
int emit_statement(const CodegenContext *ctx, CtxStatement *statement);


CompilerContext *initialize_compiler() {
    CompilerContext *ctx = calloc(sizeof(CompilerContext), 1);
    ctx->width = 64;
    ctx->height = 64;
    ctx->current_address = START_ADDRESS;

    map_create(&ctx->constants, 16);
    map_create(&ctx->globals, 16);
    ctx->load_statements = (DynamicArray) {0};
    map_create(&ctx->locals, 16);
    map_create(&ctx->proc_params, 16);
    map_create(&ctx->kept_names, 16);

    ctx->procedures = (DynamicArray) {0};
    ctx->program_nodes = (DynamicArray) {0};

    ctx->start_proc_exists = 0;
    ctx->tick_proc_exists = 0;

    ctx->current_lexer_id = 1;
    
    set_default_constants(&ctx->constants);

    return ctx;
}


int record_meta_var(CompilerContext *ctx, CtxMetaVarStatement *meta_var) {
    char *name = get_token_string(meta_var->name);
    char *value_str = get_token_string(meta_var->value);
    size_t value = atoi(value_str);

    int result = 0;
    if (strcmp(name, "width") == 0) {
        ctx->width = value;
    }
    else if (strcmp(name, "height") == 0) {
        ctx->height = value;
    }
    else {
        print_token_error(meta_var->name, "Unrecognized meta variable name");
        result = 1;
    }

    free(name);
    free(value_str);

    return result;
}


int record_constant(CompilerContext *ctx, CtxConstDefinition *const_def) {
    char *name = get_token_string(const_def->name);
    char *value_str = get_token_string(const_def->value);
    int32_t value = atoi(value_str);

    if (map_get(NULL, &ctx->constants, name) == 0) {
        print_token_error(const_def->name, "Constant with this name is already defined");
        return 1;
    }
    map_add(&ctx->constants, name, (void*) (long) value);

    free(name);
    free(value_str);

    return 0;
}


int record_load(CompilerContext *ctx, CtxLoadStatement *load_statment) {
    char *var_name = get_token_string(load_statment->name);

    map_add(&ctx->kept_names, var_name, NULL);
    free(var_name);

    return da_append(&ctx->load_statements, load_statment);
}


int get_constant(int32_t *dest, CompilerContext *ctx, const char *name) {
    void *get_dest;
    int result = map_get(&get_dest, &ctx->constants, name);
    *dest = (int32_t) (long) get_dest;
    return result;
}


// Returns the amount of space that should be reserved for this variable
size_t get_var_size(CompilerContext *ctx, const CtxVarDeclaration *var) {
    size_t var_size = 1;
    if (var->is_sized) {
        char *var_size_str = get_token_string(var->size);
        if (var->size.kind == INT) {
            var_size = atoi(var_size_str);
        }
        else {  // Constant name
            int32_t size_i32;
            get_constant(&size_i32, ctx, var_size_str);
            var_size = size_i32;
        }
        free(var_size_str);

        var_size++;  // Need one extra address for arrays
    }

    return var_size;
}


VarData *get_var_data(CompilerContext *ctx, const CtxVarDeclaration *var) {
    int32_t var_size = get_var_size(ctx, var);

    VarData *var_data = malloc(sizeof(VarData));
    var_data->address = ctx->current_address;
    var_data->size = var_size;

    return var_data;
}


int record_global(CompilerContext *ctx, CtxDeclaration *global_decl) {
    CtxDeclarationList *decl_list = global_decl->decl_list;
    for (size_t i = 0; i < decl_list->var_count; i++) {
        CtxVarDeclaration *var = decl_list->vars[i];
        char *var_name = get_token_string(var->name);
        
        if (map_get(NULL, &ctx->globals, var_name) == 0) {
            print_token_error(var->name, "Global with this name is already defined");
            free(var_name);
            return 1;
        }
        
        VarData *var_data = get_var_data(ctx, var);
        map_add(&ctx->globals, var_name, var_data);
        ctx->current_address += var_data->size;

        free(var_name);
    }

    return 0;
}


int handle_include(CompilerContext *ctx, CtxIncludeStatement *include_statement) {
    char *file_path_unstripped = get_token_string(include_statement->file_path);
    char file_path[128];

    size_t path_length = strlen(file_path_unstripped)-2;
    strncpy(file_path, file_path_unstripped+1, MIN(path_length, 127));
    file_path[path_length] = '\0';
    free(file_path_unstripped);

    if (!file_exists(file_path)) {
        print_token_error(include_statement->file_path, "File not found");
        return 1;
    }

    char *program_string;
    read_file_bytes((uint8_t**) &program_string, NULL, file_path);

    Lexer lexer = {0};
    int init_result = lexer_init(&lexer, program_string);
    lexer.id = ctx->current_lexer_id;
    ctx->current_lexer_id++;

    if (init_result) {
        fprintf(stderr, "Error initializing lexer\n");
        return 1;
    }

    CtxProgram *program = parse_program(&lexer);
    int result = initial_pass(ctx, program);

    return result;
}


int initial_pass(CompilerContext *ctx, CtxProgram *program) {
    for (size_t i = 0; i < program->header_statement_count; i++) {
        CtxHeaderStatement *statement = program->header_statements[i];
        switch (statement->kind) {
            case META_STATEMENT:
                if (record_meta_var(ctx, statement->header_statement.meta_variable)) return 1;
                break;
            case CONST_DEF:
                if (record_constant(ctx, statement->header_statement.const_definition)) return 1;
                break;
            case LOAD_STATEMENT:
                if (record_load(ctx, statement->header_statement.load_statement)) return 1;
                break;
            case GLOBAL_DECLARATION:
                if (record_global(ctx, statement->header_statement.global_declaration)) return 1;
                break;
            case INCLUDE_STATEMENT:
                if (handle_include(ctx, statement->header_statement.include_statement)) return 1;
                break;
        }
    }

    for (size_t i = 0; i < program->procedure_count; i++) {
        CtxProcedure *proc = program->procedures[i];
        da_append(&ctx->procedures, proc);
    }

    da_append(&ctx->program_nodes, program);
}


int record_locals(CompilerContext *ctx, Map *proc_locals, const char *proc_name, const CtxDeclarationList *decl_list) {
    for (size_t i = 0; i < decl_list->var_count; i++) {
        CtxVarDeclaration *var = decl_list->vars[i];
        char *var_name = get_token_string(var->name);

        VarData *var_data = get_var_data(ctx, var);
        map_add(proc_locals, var_name, var_data);

        if (proc_name != NULL) {
            // Add parameter to global namespace
            VarData *var_data_copy = get_var_data(ctx, var);
            char param_global_name[64];
            snprintf(param_global_name, 63, "%s.%lu", proc_name, i);
            map_add(&ctx->globals, param_global_name, var_data_copy);
        }

        ctx->current_address += var_data->size;

        free(var_name);
    }

    return 0;
}


int record_locals_from_statement_list(CompilerContext *ctx, Map *proc_locals, const CtxStatementList *statement_list) {
    size_t statement_count = statement_list->statement_count;
    for (size_t j = 0; j < statement_count; j++) {
            CtxStatement *statement = statement_list->statements[j];
        if (statement->kind == LOCAL_DECLARATION) {
            CtxDeclarationList *local_decl_list = statement->statement.local_declaration->decl_list;
            if (record_locals(ctx, proc_locals, NULL, local_decl_list)) return 1;
        }
        else if (statement->kind == LOOP_BLOCK) {
            CtxLoopBlock *loop_block = statement->statement.loop_block;
            if (record_locals_from_statement_list(ctx, proc_locals, loop_block->statement_list)) return 1;
        }
    }

    return 0;
}


int memory_pass(CompilerContext *ctx) {
    for (size_t i = 0; i < ctx->procedures.length; i++) {
        CtxProcedure *procedure = ctx->procedures.data[i];
        char *proc_name = get_token_string(procedure->name);

        if (strcmp(proc_name, START_PROC_NAME) == 0) {
            ctx->start_proc_exists = 1;
        }
        else if (strcmp(proc_name, TICK_PROC_NAME) == 0) {
            ctx->tick_proc_exists = 1;
        }

        Map *proc_locals = malloc(sizeof(Map));
        map_create(proc_locals, 16);
        map_add(&ctx->locals, proc_name, proc_locals);
        
        // Record parameters
        if (procedure->parameter_list != NULL) {
            record_locals(ctx, proc_locals, proc_name, procedure->parameter_list);
        }
        
        // Record other locals
        record_locals_from_statement_list(ctx, proc_locals, procedure->block->statement_list);

        free(proc_name);
    }

    return 0;
}


void emit_load_statements(CompilerContext *ctx, CodeGenerator *gen) {
    for (size_t i = 0; i < ctx->load_statements.length; i++) {
        CtxLoadStatement *load = ctx->load_statements.data[i];
        emit(gen, "load ");
        emit_token_value(gen, load->name);
        emit(gen, " ");
        emit_token_value(gen, load->data_op);
        emit(gen, " ");
        emit_token_value(gen, load->data_type);
        emit(gen, " ");
        emit_token_value(gen, load->data_string);
        emit(gen, "\n");
    }
}


void emit_array_initializers(CodeGenerator *gen, const Map *var_data_map) {
    for (size_t i = 0; i < var_data_map->capacity; i++) {
        MapNode *node = &var_data_map->data[i];
        if (node->key != NULL) {
            VarData *var_data = node->value;
            if (var_data->size > 1) {
                emit_indent(gen);
                emit(gen, "ldi %#x %#x\n", var_data->address, var_data->address+1);
            }
        }
    }
}


int emit_instruction_arg(const CodegenContext *ctx, CtxInstruction *instruction, size_t arg_index) {
    CodeGenerator *gen = ctx->gen;

    Token arg = instruction->args[arg_index];
    char *arg_value_str = get_token_string(arg);

    if (arg.kind == INT) {
        emit_token_value(gen, arg);
    }
    else {  // Name
        if (map_get(NULL, &ctx->ctx->kept_names, arg_value_str) == 0) {
            // Is a kept name
            emit(gen, "%s", arg_value_str);
        }
        else {
            void* value = NULL;
            int32_t value_int = 0;
            if (map_get(&value, &ctx->ctx->constants, arg_value_str) == 0) {
                value_int = (int32_t) (long) value;
            }
            else if (map_get(&value, &ctx->ctx->globals, arg_value_str) == 0) {
                value_int = ((VarData*) value)->address;
            }
            else if (map_get(&value, ctx->proc_locals, arg_value_str) == 0) {
                value_int = ((VarData*) value)->address;
            }
            else {
                print_token_error(arg, "Unrecognized name");
                free(arg_value_str);
                return 1;
            }
    
            emit(gen, "%#x", value_int);
        }
    }

    free(arg_value_str);
}


int emit_instruction_args(const CodegenContext *ctx, CtxInstruction *instruction) {
    for (size_t i = 0; i < instruction->arg_count; i++) {
        if (emit_instruction_arg(ctx, instruction, i)) return 1;

        if (i < instruction->arg_count-1) {
            emit(ctx->gen, " ");
        }
    }

    return 0;
}


int emit_instruction(const CodegenContext *ctx, CtxInstruction *instruction) {
    CodeGenerator *gen = ctx->gen;

    char *ins_name = get_token_string(instruction->name);

    int break_error = 0;
    int arg_error = 0;

    emit_indent(gen);

    if (strcmp(ins_name, "call") == 0) {
        emit(gen, "call ");
        emit_token_value(gen, instruction->args[0]);
    }
    else if (strcmp(ins_name, "brkeq") == 0) {
        if (ctx->break_label == NULL) break_error = 1;
        else {
            emit(gen, "cmp %#x ", EXTRA_ARITHMETIC_REGISTER);
            if (emit_instruction_args(ctx, instruction)) arg_error = 1;
            emit(gen, "\n");
            emit_indent(gen);
            emit(gen, "jnea %s %#x 1", ctx->break_label, EXTRA_ARITHMETIC_REGISTER);
        }
    }
    else if (strcmp(ins_name, "brkne") == 0) {
        if (ctx->break_label == NULL) break_error = 1;
        else {
            emit(gen, "jnea %s ", ctx->break_label);
            if (emit_instruction_args(ctx, instruction)) arg_error = 1;
        }
    }
    else if (strcmp(ins_name, "brkge") == 0) {
        if (ctx->break_label == NULL) break_error = 1;
        else {
            emit(gen, "cmp %#x ", EXTRA_ARITHMETIC_REGISTER);
            if (emit_instruction_args(ctx, instruction)) arg_error = 1;
            emit(gen, "\n");
            emit_indent(gen);
            emit(gen, "jnea %s %#x 1", ctx->break_label, EXTRA_ARITHMETIC_REGISTER);
        }
    }
    else if (strcmp(ins_name, "brkle") == 0) {
        if (ctx->break_label == NULL) break_error = 1;
        else {
            emit(gen, "cmp %#x ", EXTRA_ARITHMETIC_REGISTER);
            if (emit_instruction_args(ctx, instruction)) arg_error = 1;
            emit(gen, "\n");
            emit_indent(gen);
            emit(gen, "jnea %s %#x 2", ctx->break_label, EXTRA_ARITHMETIC_REGISTER);
        }
    }
    else if (strcmp(ins_name, "brk") == 0) {
        if (ctx->break_label == NULL) break_error = 1;
        else {
            emit(gen, "ja %s", ctx->break_label);
        }
    }
    else {  // All other instructions
        emit_token_value(gen, instruction->name);
        emit(gen, " ");
        if (emit_instruction_args(ctx, instruction)) arg_error = 1;
    }

    emit(gen, "\n");

    free(ins_name);

    if (break_error) {
        print_token_error(instruction->name, "Tried to break while not inside loop");
        return 1;
    }

    if (arg_error) {
        return 1;
    }

    return 0;
}


int emit_loop_block(const CodegenContext *ctx, CtxLoopBlock *loop_block) {
    CodeGenerator *gen = ctx->gen;

    char label_name[64], loop_label[64], break_label[64];

    snprintf(label_name, 63, "%lu_%lu_%lu", loop_block->lbracket.lexer_id, loop_block->lbracket.line_num, loop_block->lbracket.col_num);
    snprintf(loop_label, 63, "loop_%s", label_name);
    snprintf(break_label, 63, "break_%s", label_name);

    emit_indent(gen);
    emit(gen, "%s:\n", loop_label);

    gen->indent_level++;

    // Create new context to change the break label for the block
    CodegenContext new_ctx = *ctx;
    new_ctx.break_label = break_label;

    for (size_t i = 0; i < loop_block->statement_list->statement_count; i++) {
        CtxStatement *statement = loop_block->statement_list->statements[i];
        if (emit_statement(&new_ctx, statement)) return 1;
    }
    
    emit_indent(gen);
    emit(gen, "ja %s\n", loop_label);

    gen->indent_level--;

    emit_indent(gen);
    emit(gen, "%s:\n", break_label);

    return 0;
}


int emit_statement(const CodegenContext *ctx, CtxStatement *statement) {
    switch (statement->kind) {
        case INSTRUCTION:
            return emit_instruction(ctx, statement->statement.instruction);
        case LOOP_BLOCK:
            return emit_loop_block(ctx, statement->statement.loop_block);
    }

    return 0;
}


int emit_start_statements(CompilerContext *ctx, CodeGenerator *gen) {
    // Emit first frame flag
    emit_indent(gen);
    emit(gen, "ldi %#x 1\n", FIRST_FRAME_FLAG_ADDRESS);

    // Emit global array initializers
    emit_array_initializers(gen, &ctx->globals);
}


int emit_procedure(CompilerContext *ctx, CodeGenerator *gen, CtxProcedure *proc) {
    char *proc_name = get_token_string(proc->name);
    emit(gen, "\n%s:\n", proc_name);
    
    gen->indent_level++;
    
    int is_start_proc = strcmp(proc_name, START_PROC_NAME) == 0;
    int is_tick_proc = strcmp(proc_name, TICK_PROC_NAME) == 0;

    if (is_start_proc) {
        emit_start_statements(ctx, gen);
    }
    
    Map *proc_locals;
    map_get((void**) &proc_locals, &ctx->locals, proc_name);
    emit_array_initializers(gen, proc_locals);
    
    DynamicArray *proc_params;
    map_get((void**) &proc_params, &ctx->proc_params, proc_name);
    
    CodegenContext codegen_ctx = {
        .ctx = ctx,
        .gen = gen,
        .proc_name = proc_name,
        .proc_locals = proc_locals,
        .proc_params = proc_params,
        .break_label = NULL
    };
    
    size_t statement_count = proc->block->statement_list->statement_count;
    for (size_t i = 0; i < statement_count; i++) {
        CtxStatement *statement = proc->block->statement_list->statements[i];
        if (emit_statement(&codegen_ctx, statement)) return 1;
    }

    emit_indent(gen);
    if (is_start_proc) {
        emit(gen, "ja tick\n");
    }
    else if (is_tick_proc) {
        emit(gen, "ja %s\n", END_LABEL_NAME);
    }
    else {
        emit(gen, "ret\n");
    }

    gen->indent_level--;
    
    free(proc_name);

    return 0;
}


int codegen_pass(CompilerContext *ctx, const char *outfile) {
    CodeGenerator gen;
    create_code_generator(&gen, outfile);

    
    // Emit meta vars
    emit(&gen, "#width %lu\n#height %lu\n#memory %lu\n", ctx->width, ctx->height, ctx->current_address);
    
    emit_load_statements(ctx, &gen);

    // Emit start/tick callers
    emit(&gen, "jnea start %#x 1\nja tick\n", FIRST_FRAME_FLAG_ADDRESS);
    
    // Emit procedures
    for (size_t i = 0; i < ctx->procedures.length; i++) {
        CtxProcedure *proc = ctx->procedures.data[i];
        if (emit_procedure(ctx, &gen, proc)) return 1;
    }

    if (!ctx->start_proc_exists) {
        emit(&gen, "start:\n");
        gen.indent_level++;

        emit_start_statements(ctx, &gen);
        emit_indent(&gen);
        emit(&gen, "ja tick\n");

        gen.indent_level--;
    }

    if (!ctx->tick_proc_exists) {
        emit(&gen, "tick:\nja %s\n", END_LABEL_NAME);
    }

    // Emit end label
    emit(&gen, "%s:\n", END_LABEL_NAME);
    
    close_code_generator(&gen);

    return 0;
}


int cleanup_compiler(CompilerContext *ctx) {
    // Free AST nodes
    for (size_t i = 0; i < ctx->program_nodes.length; i++) {
        CtxProgram *program = ctx->program_nodes.data[i];
        free_program(program);
    }
    
    // Free constants
    map_free(&ctx->constants);
    
    // Free globals
    map_free_all(&ctx->globals);
    
    // Free load statements
    da_free(&ctx->load_statements);
    
    // Free locals
    for (size_t i = 0; i < ctx->locals.capacity; i++) {
        MapNode *node = &ctx->locals.data[i];
        if (node->key != NULL) {
            Map *proc_locals = node->value;
            map_free_all(proc_locals);
        }
    }
    map_free(&ctx->locals);

    // Free param addresses
    map_free_all_with(ctx->proc_params, da_free);

    // Free kept names
    map_free(&ctx->kept_names);

    da_free(&ctx->procedures);
    da_free(&ctx->program_nodes);
}
