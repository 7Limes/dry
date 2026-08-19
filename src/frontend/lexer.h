#ifndef _LEXER_H
#define _LEXER_H

#include <regex.h>


#define TOKEN_COUNT 16
#define IGNORED_COUNT 2


typedef enum {
    LOAD,
    DEFINE,
    INCLUDE,

    LPAREN,
    RPAREN,
    LBRACE,
    RBRACE,
    LBRACKET,
    RBRACKET,

    COMMA,
    DOLLAR_SIGN,
    HASHTAG,
    CARAT,

    NAME,
    INT,
    STRING

} TokenKind;


typedef struct {
    const char *source;
    size_t lexer_id;
    size_t index, length;
    size_t line_num, col_num;
    TokenKind kind;
} Token;


typedef struct {
    const char *source;
    size_t id;
    size_t source_length, index, prev_index;
    size_t line_num, col_num;
    regex_t token_expressions[TOKEN_COUNT];
    regex_t ignored_expressions[IGNORED_COUNT];
} Lexer;


// Initializes the lexer
int lexer_init(Lexer *lexer, const char *source);

// Returns the next token
int lexer_next(Token *token, Lexer *lexer);

// Reverts to the previous position
void lexer_prev(Lexer *lexer);

// Returns the next token without advancing
Token lexer_peek(Lexer *lexer);

int lexer_is_done(const Lexer *lexer);

char* get_token_string(Token token);

void print_token_error(Token token, const char *message);

void print_token(Token token);


#endif
