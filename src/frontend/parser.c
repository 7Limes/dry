#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include "parser.h"
#include "../globals.h"
#include "../util/da.h"
#include "../util/map.h"


typedef struct {
    Lexer *lex;
    int error;
    DynamicArray trace;
} ParserContext;


// Forward declarations
CtxStatement* parse_statement(ParserContext *ctx);
void free_statement(CtxStatement *statement);


void trace_push(ParserContext *ctx, const char *s) {
    da_append(&ctx->trace, (void*) s);
}

void trace_pop(ParserContext *ctx) {
    da_pop(&ctx->trace, NULL);
}

void print_trace(const ParserContext *ctx) {
    printf("Parser Trace:\n");
    for (size_t i = 0; i < ctx->trace.length; i++) {
        for (size_t j = 0; j < i; j++) {
            printf(" ");
        }
        printf("%ld. %s\n", i, (char*) (ctx->trace.data[i]));
    }
}


void print_lexer_end_error() {
    fprintf(stderr, "%sERROR: Reached end of token stream%s\n", COL_ERROR, COL_RESET);
}


int next_token_safe(Token *token, ParserContext *ctx, TokenKind expected_kind) {
    if (lexer_next(token, ctx->lex)) {
        print_lexer_end_error();
        print_trace(ctx);
        ctx->error = 1;
        return 1;
    }
    return token->kind != expected_kind;
}


int next_token_helper(Token *token, ParserContext *ctx, TokenKind expected_kind, int error_on_eof) {
    if (lexer_next(token, ctx->lex)) {
        if (error_on_eof) {
            print_lexer_end_error();
            print_trace(ctx);
            ctx->error = 1;
        }
        return 1;
    }

    if (token->kind != expected_kind) {
        char err_buf[128];
        snprintf(err_buf, 127, "Expected token of type %s but got %s", TOKEN_KINDS[expected_kind], TOKEN_KINDS[token->kind]);
        print_token_error(*token, err_buf);
        print_trace(ctx);
        ctx->error = 1;
        return 2;
    }

    return 0;
}


int next_token(Token *token, ParserContext *ctx, TokenKind expected_kind) {
    return next_token_helper(token, ctx, expected_kind, 1);
}

int next_token_eof_ok(Token *token, ParserContext *ctx, TokenKind expected_kind) {
    return next_token_helper(token, ctx, expected_kind, 0);
}

int next_token_n(Token *token, ParserContext *ctx, int n_expected, ...) {
    if (lexer_next(token, ctx->lex)) {
        print_lexer_end_error();
        ctx->error = 1;
        return 1;
    }

    va_list args;
    va_start(args, n_expected);

    int matched = 0;
    for (int i = 0; i < n_expected; i++) {
        TokenKind k = va_arg(args, TokenKind);
        if (token->kind == k) {
            matched = 1;
            break;
        }
    }
    va_end(args);

    if (!matched) {
        char err_buf[256];
        int off = snprintf(err_buf, sizeof(err_buf), "Expected token of type ");

        va_start(args, n_expected);
        for (int i = 0; i < n_expected && off < (int)sizeof(err_buf); i++) {
            TokenKind k = va_arg(args, TokenKind);
            const char *sep = (i == 0) ? "" : (i == n_expected - 1) ? " or " : ", ";
            off += snprintf(err_buf + off, sizeof(err_buf) - off, "%s%s", sep, TOKEN_KINDS[k]);
        }
        va_end(args);

        snprintf(err_buf + off, sizeof(err_buf) - off, " but got %s", TOKEN_KINDS[token->kind]);

        print_token_error(*token, err_buf);
        ctx->error = 1;
        return 2;
    }

    return 0;
}

int peek_next_token(Token *token, ParserContext *ctx) {
    if (lexer_next(token, ctx->lex)) {
        print_lexer_end_error();
        print_trace(ctx);
        ctx->error = 1;
        return 1;
    }

    lexer_prev(ctx->lex);
    return 0;
}


CtxVarDeclaration* parse_var_declaration(ParserContext *ctx) {
    trace_push(ctx, "var_declaration");

    Token name;
    if (next_token(&name, ctx, NAME)) {trace_pop(ctx); return NULL;}

    int is_sized = 0;
    Token size = {0};

    Token peek_token;
    if (peek_next_token(&peek_token, ctx)) {trace_pop(ctx); return NULL;}

    if (peek_token.kind == LBRACKET) {
        next_token_safe(&peek_token, ctx, LBRACKET);
        Token rbracket;
        if (next_token_n(&size, ctx, 2, INT, NAME)) {trace_pop(ctx); return NULL;}
        if (next_token(&rbracket, ctx, RBRACKET)) {trace_pop(ctx); return NULL;}
        is_sized = 1;
    }

    CtxVarDeclaration *var_decl = malloc(sizeof(CtxVarDeclaration));
    var_decl->is_sized = is_sized;
    var_decl->name = name;
    var_decl->size = size;

    trace_pop(ctx);

    return var_decl;
}


CtxDeclarationList* parse_declaration_list(ParserContext *ctx, int required) {
    trace_push(ctx, "declaration_list");

    Token lparen;
    if (required) {
        if (next_token(&lparen, ctx, LPAREN)) {trace_pop(ctx); return NULL;}
    }
    else {
        if (next_token_safe(&lparen, ctx, LPAREN)) {
            lexer_prev(ctx->lex);
            trace_pop(ctx);
            return NULL;
        };
    }

    DynamicArray vars = {0};

    while (1) {
        CtxVarDeclaration *var_decl = parse_var_declaration(ctx);
        if (ctx->error) {
            da_free_all(&vars);
            trace_pop(ctx);
            return NULL;
        }

        da_append(&vars, var_decl);

        Token peek_token;
        if (peek_next_token(&peek_token, ctx)) {trace_pop(ctx); return NULL;}

        if (peek_token.kind == COMMA) {
            next_token_safe(&peek_token, ctx, COMMA);
            continue;
        }
        else if (peek_token.kind == RPAREN) {
            next_token_safe(&peek_token, ctx, RPAREN);
            break;
        }
        
        // Error
        da_free_all(&vars);
        print_token_error(lparen, "Expected comma or right parenthesis");
        ctx->error = 1;
        trace_pop(ctx);
        return NULL;
    }

    CtxDeclarationList *decl_list = malloc(sizeof(CtxDeclarationList));
    decl_list->var_count = vars.length;
    decl_list->vars = (CtxVarDeclaration**) vars.data;

    trace_pop(ctx);

    return decl_list;
}

void free_declaration_list(CtxDeclarationList *decl_list) {
    free(decl_list->vars);
    free(decl_list);
}


CtxStatementList* parse_statement_list(ParserContext *ctx, TokenKind closing_token) {
    trace_push(ctx, "statement_list");

    DynamicArray statements = {0};

    while (1) {
        Token closing;
        if (next_token_safe(&closing, ctx, closing_token) == 0) {
            break;
        }
        lexer_prev(ctx->lex);

        CtxStatement *statement = parse_statement(ctx);
        if (ctx->error) {
            da_free_all_with(statements, free_statement);
            trace_pop(ctx);
            return NULL;
        }

        da_append(&statements, statement);
    }

    CtxStatementList *list = malloc(sizeof(CtxStatementList));
    list->statement_count = statements.length;
    list->statements = (CtxStatement**) statements.data;

    trace_pop(ctx);

    return list;
}


void free_statement_list(CtxStatementList *list) {
    for (size_t i = 0; i < list->statement_count; i++) {
        free_statement(list->statements[i]);
    }
    free(list->statements);
    free(list);
}


CtxInstruction* parse_instruction(ParserContext *ctx) {
    trace_push(ctx, "instruction");

    Token name;
    if (next_token_safe(&name, ctx, NAME)) {
        lexer_prev(ctx->lex);
        trace_pop(ctx);
        return NULL;
    }

    size_t arg_count;
    char *instruction_name = get_token_string(name);
    int result = map_get((void**) &arg_count, &INSTRUCTION_LOOKUP, instruction_name);
    if (result) {
        ctx->error = 1;
        char err_buf[128];
        snprintf(err_buf, 127, "Unrecognized instruction \"%s\"", instruction_name);
        print_token_error(name, err_buf);
        free(instruction_name);
        trace_pop(ctx);
        return NULL;
    }

    free(instruction_name);

    Token *args = calloc(sizeof(Token), arg_count);

    for (size_t i = 0; i < arg_count; i++) {
        Token arg;
        if (next_token_n(&arg, ctx, 2, INT, NAME)) {trace_pop(ctx); return NULL;}
        args[i] = arg;
    }

    trace_pop(ctx);

    CtxInstruction *instruction = malloc(sizeof(CtxInstruction));
    instruction->name = name;
    instruction->arg_count = arg_count;
    instruction->args = args;

    return instruction;
}


void free_instruction(CtxInstruction *instruction) {
    free(instruction->args);
    free(instruction);
}


CtxDeclaration* parse_declaration(ParserContext *ctx) {
    trace_push(ctx, "declaration");

    Token dollar;
    if (next_token_safe(&dollar, ctx, DOLLAR_SIGN)) {
        lexer_prev(ctx->lex);
        trace_pop(ctx);
        return NULL;
    } 

    CtxDeclarationList *decl_list = parse_declaration_list(ctx, 1);
    if (ctx->error) {trace_pop(ctx); return NULL;}

    CtxDeclaration *local_declaration = malloc(sizeof(CtxDeclaration));
    local_declaration->decl_list = decl_list;

    trace_pop(ctx);

    return local_declaration;
}


void free_declaration(CtxDeclaration *decl) {
    free_declaration_list(decl->decl_list);
    free(decl);
}


CtxLoopBlock* parse_loop_block(ParserContext *ctx) {
    trace_push(ctx, "loop_block");

    Token lbracket;
    if (next_token_safe(&lbracket, ctx, LBRACKET)) {
        lexer_prev(ctx->lex);
        trace_pop(ctx);
        return NULL;
    }

    CtxStatementList *statement_list = parse_statement_list(ctx, RBRACKET);
    if (ctx->error) {trace_pop(ctx); return NULL;}

    CtxLoopBlock *block = malloc(sizeof(CtxLoopBlock));
    block->lbracket = lbracket;
    block->statement_list = statement_list;

    trace_pop(ctx);

    return block;
}


void free_loop_block(CtxLoopBlock *block) {
    free_statement_list(block->statement_list);
    free(block);
}

CtxProcArgument* parse_proc_argument(ParserContext *ctx) {
    trace_push(ctx, "proc_argument");

    Token name_or_int;
    int use_ldi = 0;

    Token first_token;
    if (next_token_n(&first_token, ctx, 3, NAME, INT, CARAT)) {
        lexer_prev(ctx->lex);
        trace_pop(ctx);
        return NULL;
    };

    if (first_token.kind == CARAT) {
        use_ldi = 1;
        if (next_token_n(&name_or_int, ctx, 2, NAME, INT)) {
            lexer_prev(ctx->lex);
            trace_pop(ctx);
            return NULL;
        };
    }
    else {
        name_or_int = first_token;
    }

    CtxProcArgument *arg = malloc(sizeof(CtxProcArgument));
    arg->name_or_int = name_or_int;
    arg->use_ldi = use_ldi;

    trace_pop(ctx);

    return arg;
}

CtxProcArgumentList* parse_proc_argument_list(ParserContext *ctx) {
    trace_push(ctx, "proc_argument_list");

    Token lparen;
    if (next_token_safe(&lparen, ctx, LPAREN)) {
        lexer_prev(ctx->lex);
        trace_pop(ctx);
        return NULL;
    };

    int loop = 1;
    Token rparen;
    if (peek_next_token(&rparen, ctx)) {
        trace_pop(ctx);
        return NULL;
    }
    else if (rparen.kind == RPAREN) {
        lexer_next(&rparen, ctx->lex);
        loop = 0;
    }

    DynamicArray args = {0};

    while (loop) {
        CtxProcArgument *arg = parse_proc_argument(ctx);
        if (ctx->error) {
            da_free_all(&args);
            trace_pop(ctx);
            return NULL;
        }

        da_append(&args, arg);

        Token peek_token;
        if (peek_next_token(&peek_token, ctx)) {trace_pop(ctx); return NULL;}

        if (peek_token.kind == COMMA) {
            next_token_safe(&peek_token, ctx, COMMA);
            continue;
        }
        else if (peek_token.kind == RPAREN) {
            next_token_safe(&peek_token, ctx, RPAREN);
            break;
        }
        
        // Error
        da_free_all(&args);
        print_token_error(lparen, "Expected comma or right parenthesis");
        ctx->error = 1;
        trace_pop(ctx);
        return NULL;
    }

    CtxProcArgumentList *arg_list = malloc(sizeof(CtxProcArgumentList));
    arg_list->arg_count = args.length;
    arg_list->args = (CtxProcArgument**) args.data;

    trace_pop(ctx);

    return arg_list;
}

CtxProcCall* parse_proc_call(ParserContext *ctx) {
    trace_push(ctx, "proc_call");

    size_t saved_index = ctx->lex->index;
    Token name;
    if (next_token_safe(&name, ctx, NAME)) {
        lexer_prev(ctx->lex);
        trace_pop(ctx);
        return NULL;
    }

    CtxProcArgumentList *arg_list = parse_proc_argument_list(ctx);
    if (ctx->error || arg_list == NULL) {
        ctx->lex->index = saved_index;
        trace_pop(ctx);
        return NULL;
    }

    CtxProcCall *proc_call = malloc(sizeof(CtxProcCall));
    proc_call->name = name;
    proc_call->args = arg_list;

    trace_pop(ctx);

    return proc_call;
}


void free_proc_call(CtxProcCall *proc_call) {
    free(proc_call->args->args);
}


CtxStatement* parse_statement(ParserContext *ctx) {
    trace_push(ctx, "statement");

    CtxStatement *statement = NULL;

    CtxProcCall *proc_call = parse_proc_call(ctx);
    if (ctx->error) goto done;
    if (proc_call) {
        statement = malloc(sizeof(CtxStatement));
        statement->kind = PROC_CALL;
        statement->statement.proc_call = proc_call;
        goto done;
    } 

    CtxInstruction *instruction = parse_instruction(ctx);
    if (ctx->error) goto done;
    if (instruction) {
        statement = malloc(sizeof(CtxStatement));
        statement->kind = INSTRUCTION;
        statement->statement.instruction = instruction;
        goto done;
    }

    CtxDeclaration *local_declaration = parse_declaration(ctx);
    if (ctx->error) goto done;
    if (local_declaration) {
        statement = malloc(sizeof(CtxStatement));
        statement->kind = LOCAL_DECLARATION;
        statement->statement.local_declaration = local_declaration;
        goto done;
    }

    CtxLoopBlock *loop_block = parse_loop_block(ctx);
    if (ctx->error) goto done;
    if (loop_block) {
        statement = malloc(sizeof(CtxStatement));
        statement->kind = LOOP_BLOCK;
        statement->statement.loop_block = loop_block;
        goto done;
    }

    if (statement == NULL) {
        Token err_token;
        if (peek_next_token(&err_token, ctx) == 0) {
            print_token_error(err_token, "Expected instruction, local declaration, loop block, or procedure call");
            ctx->error = 1;
        }
    }

    done:

    trace_pop(ctx);

    return statement;
}


void free_statement(CtxStatement *statement) {
    switch (statement->kind) {
        case INSTRUCTION:
            free_instruction(statement->statement.instruction);
            break;
        case LOCAL_DECLARATION:
            free_declaration(statement->statement.local_declaration);
            break;
        case LOOP_BLOCK:
            free_loop_block(statement->statement.loop_block);
            break;
        case PROC_CALL:
            free_proc_call(statement->statement.proc_call);
            break;
    }
    
    free(statement);
}


CtxBlock* parse_block(ParserContext *ctx) {
    trace_push(ctx, "block");

    Token lbrace;
    if (next_token(&lbrace, ctx, LBRACE)) {trace_pop(ctx); return NULL;}

    CtxStatementList *statement_list = parse_statement_list(ctx, RBRACE);
    if (ctx->error) {trace_pop(ctx); return NULL;}

    CtxBlock *block = malloc(sizeof(CtxBlock));
    block->statement_list = statement_list;

    trace_pop(ctx);

    return block;
}


void free_block(CtxBlock *block) {
    free_statement_list(block->statement_list);
    free(block);
}


CtxProcedure* parse_procedure(ParserContext *ctx) {
    trace_push(ctx, "procedure");

    Token proc_name;
    int next_result = next_token_eof_ok(&proc_name, ctx, NAME);
    if (next_result == 1) {trace_pop(ctx); return NULL;}  // End of file
    else if (next_result == 2) {trace_pop(ctx); return NULL;}

    CtxDeclarationList *decl_list = parse_declaration_list(ctx, 0);
    if (ctx->error) {trace_pop(ctx); return NULL;}

    CtxBlock *block = parse_block(ctx);
    if (ctx->error) {trace_pop(ctx); return NULL;}

    CtxProcedure *proc = malloc(sizeof(CtxProcedure));
    proc->name = proc_name;
    proc->parameter_list = decl_list;
    proc->block = block;

    trace_pop(ctx);

    return proc;
}


void free_procedure(CtxProcedure *proc) {
    if (proc->parameter_list != NULL) {
        free_declaration_list(proc->parameter_list);
    }
    free_block(proc->block);
    free(proc);
}


CtxMetaVarStatement* parse_meta_var_statement(ParserContext *ctx) {
    trace_push(ctx, "meta_var_statement");

    Token hashtag;
    if (next_token_safe(&hashtag, ctx, HASHTAG)) {
        lexer_prev(ctx->lex);
        trace_pop(ctx);
        return NULL;
    }
    
    Token name, value;
    if (next_token(&name, ctx, NAME)) {trace_pop(ctx); return NULL;}
    if (next_token(&value, ctx, INT)) {trace_pop(ctx); return NULL;}
    
    CtxMetaVarStatement *meta_var = malloc(sizeof(CtxMetaVarStatement));
    meta_var->name = name;
    meta_var->value = value;

    trace_pop(ctx);

    return meta_var;
}


CtxConstDefinition* parse_const_definition(ParserContext *ctx) {
    trace_push(ctx, "const_definition");

    Token define;
    if (next_token_safe(&define, ctx, DEFINE)) {
        lexer_prev(ctx->lex);
        trace_pop(ctx);
        return NULL;
    }

    Token name, value;
    if (next_token(&name, ctx, NAME)) {trace_pop(ctx); return NULL;}
    if (next_token(&value, ctx, INT)) {trace_pop(ctx); return NULL;}
    
    CtxConstDefinition *const_def = malloc(sizeof(CtxConstDefinition));
    const_def->name = name;
    const_def->value = value;

    trace_pop(ctx);

    return const_def;
}


CtxLoadStatement* parse_load_statement(ParserContext *ctx) {
    trace_push(ctx, "load_statement");

    Token load;
    if (next_token_safe(&load, ctx, LOAD)) {
        lexer_prev(ctx->lex);
        trace_pop(ctx);
        return NULL;
    }

    Token name, data_op, data_type, data_string;
    if (next_token(&name, ctx, NAME)) {trace_pop(ctx); return NULL;}
    if (next_token(&data_op, ctx, NAME)) {trace_pop(ctx); return NULL;}
    if (next_token(&data_type, ctx, NAME)) {trace_pop(ctx); return NULL;}
    if (next_token(&data_string, ctx, STRING)) {trace_pop(ctx); return NULL;}

    CtxLoadStatement *load_statement = malloc(sizeof(CtxLoadStatement));

    load_statement->name = name;
    load_statement->data_op = data_op;
    load_statement->data_type = data_type;
    load_statement->data_string = data_string;

    trace_pop(ctx);

    return load_statement;
}


CtxIncludeStatement* parse_include_statement(ParserContext *ctx) {
    trace_push(ctx, "include_statement");

    Token include;
    if (next_token_safe(&include, ctx, INCLUDE)) {{
        lexer_prev(ctx->lex);
        trace_pop(ctx);
        return NULL;
    }}

    Token file_path;
    if (next_token(&file_path, ctx, STRING)) {trace_pop(ctx); return NULL;}

    CtxIncludeStatement *include_statement = malloc(sizeof(CtxIncludeStatement));
    include_statement->file_path = file_path;

    trace_pop(ctx);

    return include_statement;
}


CtxHeaderStatement* parse_header_statement(ParserContext *ctx) {
    trace_push(ctx, "header_statement");

    CtxHeaderStatement *statement = NULL;

    CtxMetaVarStatement *meta_var = parse_meta_var_statement(ctx);
    if (ctx->error) goto done;
    if (meta_var) {
        statement = malloc(sizeof(CtxHeaderStatement));
        statement->kind = META_STATEMENT;
        statement->header_statement.meta_variable = meta_var;
        goto done;
    }

    CtxConstDefinition *const_def = parse_const_definition(ctx);
    if (ctx->error) goto done;
    if (const_def) {
        statement = malloc(sizeof(CtxHeaderStatement));
        statement->kind = CONST_DEF;
        statement->header_statement.const_definition = const_def;
        goto done;
    }

    CtxLoadStatement *load_statement = parse_load_statement(ctx);
    if (ctx->error) goto done;
    if (load_statement) {
        statement = malloc(sizeof(CtxHeaderStatement));
        statement->kind = LOAD_STATEMENT;
        statement->header_statement.load_statement = load_statement;
        goto done;
    }

    CtxDeclaration *global_declaration = parse_declaration(ctx);
    if (ctx->error) goto done;
    if (global_declaration) {
        statement = malloc(sizeof(CtxHeaderStatement));
        statement->kind = GLOBAL_DECLARATION;
        statement->header_statement.global_declaration = global_declaration;
        goto done;
    }

    CtxIncludeStatement *include_statement = parse_include_statement(ctx);
    if (ctx->error) goto done;
    if (include_statement) {
        statement = malloc(sizeof(CtxHeaderStatement));
        statement->kind = INCLUDE_STATEMENT;
        statement->header_statement.include_statement = include_statement;
        goto done;
    }

    done:

    trace_pop(ctx);

    return statement;
}


void free_header_statement(CtxHeaderStatement *statement) {
    if (statement->kind == GLOBAL_DECLARATION) {
        free_declaration(statement->header_statement.global_declaration);
    }
    else {
        // This is fine because it's a union of only pointers
        free(statement->header_statement.meta_variable);
    }

    free(statement);
}


CtxProgram* parse_program(Lexer *lex) {
    ParserContext ctx = {
        .lex = lex,
        .error = 0,
        .trace = {0}
    };
    trace_push(&ctx, "program");

    DynamicArray statements = {0};
    while (1) {
        CtxHeaderStatement *statement = parse_header_statement(&ctx);
        if (ctx.error) {
            da_free(&statements);
            trace_pop(&ctx);
            return NULL;
        }
        if (statement == NULL) {
            break;
        }
        da_append(&statements, statement);
    }

    DynamicArray procedures = {0};
    while (1) {
        CtxProcedure *proc = parse_procedure(&ctx);
        if (ctx.error) {
            da_free(&procedures);
            trace_pop(&ctx);
            return NULL;
        }
        if (proc == NULL) {
            break;
        }
        da_append(&procedures, proc);
        if (lexer_is_done(ctx.lex)) {
            break;
        }
    }

    CtxProgram *program = malloc(sizeof(CtxProgram));
    program->header_statement_count = statements.length;
    program->header_statements = (CtxHeaderStatement**) statements.data;
    program->procedure_count = procedures.length;
    program->procedures = (CtxProcedure**) procedures.data;

    trace_pop(&ctx);

    return program;
}


void free_program(CtxProgram *program) {
    for (size_t i = 0; i < program->header_statement_count; i++) {
        free_header_statement(program->header_statements[i]);
    }
    for (size_t i = 0; i < program->procedure_count; i++) {
        free_procedure(program->procedures[i]);
    }

    free(program->header_statements);
    free(program->procedures);
    free(program);
}
