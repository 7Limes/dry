#ifndef _PARSER_H
#define _PARSER_H

#include "lexer.h"


// Forward declarations
typedef struct CtxStatementList CtxStatementList;


// Ex: `#width 100`
typedef struct {
    Token name, value;
} CtxMetaVarStatement;


// Ex: `declare MYCONST 10`
typedef struct {
    Token name, value;
} CtxConstDefinition;


// Ex: `load MYFILE raw file "file.dat"`
typedef struct {
    Token name, data_op, data_type, data_string;
} CtxLoadStatement;


// Ex: `include "dir/myfile.dry"`
typedef struct {
    Token file_path;
} CtxIncludeStatement;


// A declaration for a single var
// Ex: `var1` or `var2[10]`
typedef struct {
    int is_sized;
    Token name, size;
} CtxVarDeclaration;


// Ex: `(var1, var2[10], ...)`
typedef struct {
    size_t var_count;
    CtxVarDeclaration **vars;
} CtxDeclarationList;


// Ex: `$ (var1, var2[10], ...)`
typedef struct {
    CtxDeclarationList *decl_list;
} CtxDeclaration;


typedef enum {
    META_STATEMENT,
    CONST_DEF,
    LOAD_STATEMENT,
    GLOBAL_DECLARATION,
    INCLUDE_STATEMENT
} HeaderStatementKind;

typedef union {
    CtxMetaVarStatement *meta_variable;
    CtxConstDefinition *const_definition;
    CtxLoadStatement *load_statement;
    CtxDeclaration *global_declaration;
    CtxIncludeStatement *include_statement;
} HeaderStatementUnion;

typedef struct {
    HeaderStatementKind kind;
    HeaderStatementUnion header_statement;
} CtxHeaderStatement;


// Ex: `add value1 value2 5`
typedef struct {
    Token name;
    size_t arg_count;
    Token *args;
} CtxInstruction;


// Ex: `[ (statements...) ]`
typedef struct {
    Token lbracket;
    CtxStatementList *statement_list;
} CtxLoopBlock;


typedef struct {
    Token name_or_int;
    int use_ldi;
} CtxProcArgument;

typedef struct {
    size_t arg_count;
    CtxProcArgument **args;
} CtxProcArgumentList;

typedef struct {
    Token name;
    CtxProcArgumentList *args;
} CtxProcCall;


typedef enum {
    LOCAL_DECLARATION,
    INSTRUCTION,
    LOOP_BLOCK,
    PROC_CALL
} StatementKind;

typedef union {
    CtxInstruction *instruction;
    CtxDeclaration *local_declaration;
    CtxLoopBlock *loop_block;
    CtxProcCall *proc_call;
} StatementUnion;

typedef struct {
    StatementKind kind;
    StatementUnion statement;
} CtxStatement;


struct CtxStatementList {
    size_t statement_count;
    CtxStatement **statements;
};


typedef struct {
    CtxStatementList *statement_list;
} CtxBlock;

typedef struct {
    Token name;
    CtxDeclarationList *parameter_list;
    CtxBlock *block;
} CtxProcedure;


typedef struct {
    size_t header_statement_count;
    CtxHeaderStatement **header_statements;
    size_t procedure_count;
    CtxProcedure **procedures;
} CtxProgram;


CtxProgram* parse_program(Lexer *lex);
void free_program(CtxProgram *program);


#endif
