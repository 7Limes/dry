#include <stdlib.h>
#include <stdio.h>
#include "parser.h"
#include "../globals.h"
#include "../util/da.h"
#include "../util/map.h"


typedef struct {
    Lexer *lex;
    int error;
} ParserContext;


// Forward declarations
CtxStatement* parse_statement(ParserContext *ctx);
void free_statement(CtxStatement *statement);


int next_token_safe(Token *token, ParserContext *ctx, TokenKind expected_kind) {
    lexer_next(token, ctx->lex);
    return token->kind != expected_kind;
}


int next_token(Token *token, ParserContext *ctx, TokenKind expected_kind) {
    lexer_next(token, ctx->lex);

    if (token->kind != expected_kind) {
        char err_buf[128];
        snprintf(err_buf, 127, "Expected token of type %s but got %s", TOKEN_KINDS[expected_kind], TOKEN_KINDS[token->kind]);
        print_token_error(*token, err_buf);
        ctx->error = 1;
        return 1;
    }

    return 0;
}

int next_token2(Token *token, ParserContext *ctx, TokenKind expected1, TokenKind expected2) {
    lexer_next(token, ctx->lex);

    if (token->kind != expected1 && token->kind != expected2) {
        char err_buf[128];
        snprintf(err_buf, 127, "Expected token of type %s or %s but got %s", TOKEN_KINDS[expected1], TOKEN_KINDS[expected2], TOKEN_KINDS[token->kind]);
        print_token_error(*token, err_buf);
        ctx->error = 1;
        return 1;
    }

    return 0;
}


CtxVarDeclaration* parse_var_declaration(ParserContext *ctx) {
    Token name;
    if (next_token(&name, ctx, NAME)) return NULL;

    int is_sized = 0;
    Token size = {0};

    Token peek_token;
    lexer_next(&peek_token, ctx->lex);
    if (peek_token.kind == LBRACKET) {
        Token rbracket;
        if (next_token2(&size, ctx, INT, NAME)) return NULL;
        if (next_token(&rbracket, ctx, RBRACKET)) return NULL;
        is_sized = 1;
    }
    else {
        lexer_prev(ctx->lex);
    }

    CtxVarDeclaration *var_decl = malloc(sizeof(CtxVarDeclaration));
    var_decl->is_sized = is_sized;
    var_decl->name = name;
    var_decl->size = size;

    return var_decl;
}


CtxDeclarationList* parse_declaration_list(ParserContext *ctx, int required) {
    Token lparen;
    if (required) {
        if (next_token(&lparen, ctx, LPAREN)) return NULL;
    }
    else {
        if (next_token_safe(&lparen, ctx, LPAREN)) {
            lexer_prev(ctx->lex);
            return NULL;
        };
    }

    DynamicArray vars = {0};

    while (1) {
        CtxVarDeclaration *var_decl = parse_var_declaration(ctx);
        if (ctx->error) {
            da_free_all(&vars);
            return NULL;
        }

        da_append(&vars, var_decl);

        Token peek_token;
        lexer_next(&peek_token, ctx->lex);
        if (peek_token.kind == COMMA) {
            continue;
        }
        else if (peek_token.kind == RPAREN) {
            break;
        }
        
        // Error
        da_free_all(&vars);
        print_token_error(lparen, "Expected comma or right parenthesis");
        ctx->error = 1;
        return NULL;
    }

    CtxDeclarationList *decl_list = malloc(sizeof(CtxDeclarationList));
    decl_list->var_count = vars.length;
    decl_list->vars = (CtxVarDeclaration**) vars.data;

    return decl_list;
}

void free_declaration_list(CtxDeclarationList *decl_list) {
    free(decl_list->vars);
    free(decl_list);
}


CtxStatementList* parse_statement_list(ParserContext *ctx, TokenKind closing_token) {
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
            return NULL;
        }

        da_append(&statements, statement);
    }

    CtxStatementList *list = malloc(sizeof(CtxStatementList));
    list->statement_count = statements.length;
    list->statements = (CtxStatement**) statements.data;    
}


void free_statement_list(CtxStatementList *list) {
    for (size_t i = 0; i < list->statement_count; i++) {
        free_statement(list->statements[i]);
    }
    free(list->statements);
    free(list);
}


CtxInstruction* parse_instruction(ParserContext *ctx) {
    Token name;
    if (next_token_safe(&name, ctx, NAME)) {
        lexer_prev(ctx->lex);
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
        return NULL;
    }

    free(instruction_name);

    Token *args = calloc(sizeof(Token), arg_count);

    for (size_t i = 0; i < arg_count; i++) {
        Token arg;
        if (next_token2(&arg, ctx, INT, NAME)) return NULL;
        args[i] = arg;
    }

    CtxInstruction *instruction = malloc(sizeof(CtxInstruction));
    instruction->name = name;
    instruction->arg_count = arg_count;
    instruction->args = args;
}


void free_instruction(CtxInstruction *instruction) {
    free(instruction->args);
    free(instruction);
}


CtxDeclaration* parse_declaration(ParserContext *ctx) {
    Token dollar;
    if (next_token_safe(&dollar, ctx, DOLLAR_SIGN)) {
        lexer_prev(ctx->lex);
        return NULL;
    } 

    CtxDeclarationList *decl_list = parse_declaration_list(ctx, 1);
    if (ctx->error) return NULL;

    CtxDeclaration *local_declaration = malloc(sizeof(CtxDeclaration));
    local_declaration->decl_list = decl_list;

    return local_declaration;
}


void free_declaration(CtxDeclaration *decl) {
    free_declaration_list(decl->decl_list);
    free(decl);
}


CtxLoopBlock* parse_loop_block(ParserContext *ctx) {
    Token lbracket;
    if (next_token_safe(&lbracket, ctx, LBRACKET)) {
        lexer_prev(ctx->lex);
        return NULL;
    }

    CtxStatementList *statement_list = parse_statement_list(ctx, RBRACKET);
    if (ctx->error) return NULL;

    CtxLoopBlock *block = malloc(sizeof(CtxLoopBlock));
    block->lbracket = lbracket;
    block->statement_list = statement_list;

    return block;
}


void free_loop_block(CtxLoopBlock *block) {
    free_statement_list(block->statement_list);
    free(block);
}


CtxStatement* parse_statement(ParserContext *ctx) {
    CtxInstruction *instruction = parse_instruction(ctx);
    if (ctx->error) return NULL;
    if (instruction) {
        CtxStatement *statement = malloc(sizeof(CtxStatement));
        statement->kind = INSTRUCTION;
        statement->statement.instruction = instruction;
        return statement;
    }

    CtxDeclaration *local_declaration = parse_declaration(ctx);
    if (ctx->error) return NULL;
    if (local_declaration) {
        CtxStatement *statement = malloc(sizeof(CtxStatement));
        statement->kind = LOCAL_DECLARATION;
        statement->statement.local_declaration = local_declaration;
        return statement;
    }

    CtxLoopBlock *loop_block = parse_loop_block(ctx);
    if (ctx->error) return NULL;
    if (loop_block) {
        CtxStatement *statement = malloc(sizeof(CtxStatement));
        statement->kind = LOOP_BLOCK;
        statement->statement.loop_block = loop_block;
        return statement;
    }

    Token err_token;
    lexer_next(&err_token, ctx->lex);
    print_token_error(err_token, "Expected instruction, local declaration, or loop block");
    ctx->error = 1;

    return NULL;
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
    }
    
    free(statement);
}


CtxBlock* parse_block(ParserContext *ctx) {
    Token lbrace;
    if (next_token(&lbrace, ctx, LBRACE)) return NULL;

    CtxStatementList *statement_list = parse_statement_list(ctx, RBRACE);
    if (ctx->error) return NULL;

    CtxBlock *block = malloc(sizeof(CtxBlock));
    block->statement_list = statement_list;

    return block;
}


void free_block(CtxBlock *block) {
    free_statement_list(block->statement_list);
    free(block);
}


CtxProcedure* parse_procedure(ParserContext *ctx) {
    Token proc_name;
    if (next_token(&proc_name, ctx, NAME)) return NULL;

    CtxDeclarationList *decl_list = parse_declaration_list(ctx, 0);
    if (ctx->error) return NULL;

    CtxBlock *block = parse_block(ctx);
    if (ctx->error) return NULL;

    CtxProcedure *proc = malloc(sizeof(CtxProcedure));
    proc->name = proc_name;
    proc->parameter_list = decl_list;
    proc->block = block;

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
    Token hashtag;
    if (next_token_safe(&hashtag, ctx, HASHTAG)) {
        lexer_prev(ctx->lex);
        return NULL;
    }
    
    Token name, value;
    if (next_token(&name, ctx, NAME)) return NULL;
    if (next_token(&value, ctx, INT)) return NULL;
    
    CtxMetaVarStatement *meta_var = malloc(sizeof(CtxMetaVarStatement));
    meta_var->name = name;
    meta_var->value = value;

    return meta_var;
}


CtxConstDefinition* parse_const_definition(ParserContext *ctx) {
    Token define;
    if (next_token_safe(&define, ctx, DEFINE)) {
        lexer_prev(ctx->lex);
        return NULL;
    }

    Token name, value;
    if (next_token(&name, ctx, NAME)) return NULL;
    if (next_token(&value, ctx, INT)) return NULL;
    
    CtxConstDefinition *const_def = malloc(sizeof(CtxConstDefinition));
    const_def->name = name;
    const_def->value = value;

    return const_def;
}


CtxLoadStatement* parse_load_statement(ParserContext *ctx) {
    Token load;
    if (next_token_safe(&load, ctx, LOAD)) {
        lexer_prev(ctx->lex);
        return NULL;
    }

    Token name, data_op, data_type, data_string;
    if (next_token(&name, ctx, NAME)) return NULL;
    if (next_token(&data_op, ctx, NAME)) return NULL;
    if (next_token(&data_type, ctx, NAME)) return NULL;
    if (next_token(&data_string, ctx, STRING)) return NULL;

    CtxLoadStatement *load_statement = malloc(sizeof(CtxLoadStatement));

    load_statement->name = name;
    load_statement->data_op = data_op;
    load_statement->data_type = data_type;
    load_statement->data_string = data_string;
}


CtxIncludeStatement* parse_include_statement(ParserContext *ctx) {
    Token include;
    if (next_token_safe(&include, ctx, INCLUDE)) {{
        lexer_prev(ctx->lex);
        return NULL;
    }}

    Token file_path;
    if (next_token(&file_path, ctx, STRING)) return NULL;

    CtxIncludeStatement *include_statement = malloc(sizeof(CtxIncludeStatement));
    include_statement->file_path = file_path;

    return include_statement;
}


CtxHeaderStatement* parse_header_statement(ParserContext *ctx) {
    CtxMetaVarStatement *meta_var = parse_meta_var_statement(ctx);
    if (ctx->error) return NULL;
    if (meta_var) {
        CtxHeaderStatement *statement = malloc(sizeof(CtxHeaderStatement));
        statement->kind = META_STATEMENT;
        statement->header_statement.meta_variable = meta_var;
        return statement;
    }

    CtxConstDefinition *const_def = parse_const_definition(ctx);
    if (ctx->error) return NULL;
    if (const_def) {
        CtxHeaderStatement *statement = malloc(sizeof(CtxHeaderStatement));
        statement->kind = CONST_DEF;
        statement->header_statement.const_definition = const_def;
        return statement;
    }

    CtxLoadStatement *load_statement = parse_load_statement(ctx);
    if (ctx->error) return NULL;
    if (load_statement) {
        CtxHeaderStatement *statement = malloc(sizeof(CtxHeaderStatement));
        statement->kind = LOAD_STATEMENT;
        statement->header_statement.load_statement = load_statement;
        return statement;
    }

    CtxDeclaration *global_declaration = parse_declaration(ctx);
    if (ctx->error) return NULL;
    if (global_declaration) {
        CtxHeaderStatement *statement = malloc(sizeof(CtxHeaderStatement));
        statement->kind = GLOBAL_DECLARATION;
        statement->header_statement.global_declaration = global_declaration;
        return statement;
    }

    CtxIncludeStatement *include_statement = parse_include_statement(ctx);
    if (ctx->error) return NULL;
    if (include_statement) {
        CtxHeaderStatement *statement = malloc(sizeof(CtxHeaderStatement));
        statement->kind = INCLUDE_STATEMENT;
        statement->header_statement.include_statement = include_statement;
        return statement;
    }

    return NULL;
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
        .error = 0
    };

    DynamicArray statements = {0};
    while (1) {
        CtxHeaderStatement *statement = parse_header_statement(&ctx);
        if (ctx.error) {
            da_free(&statements);
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
            return NULL;
        }
        if (proc == NULL) {
            // TODO: ERROR HERE
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
