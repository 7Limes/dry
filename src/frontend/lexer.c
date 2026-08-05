#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <regex.h>
#include "lexer.h"
#include "../globals.h"


const char *RAW_TOKEN_EXPRESSIONS[] = {
    "^[[:space:]]+",
    "^//.*$",

    "^load",
    "^define",
    "^include",

    "^\\(",
    "^\\)",
    "^\\{",
    "^\\}",
    "^\\[",
    "^\\]",

    "^,",
    "^\\$",
    "^#",

    "^[A-Za-z_][A-Za-z0-9_\\.]*",
    "^-?((0x[0-9A-Fa-f]+)|(0b[01]+)|([0-9]+))",
    "^\"[^\"]*\"",
};


void count_lines_cols(size_t *lines, size_t *cols, const char *str, size_t length) {
    *lines = 0;
    *cols = 0;
    for (size_t i = 0; i < length; i++) {
        if (str[i] == '\n') {
            (*lines)++;
            *cols = 0;
        }
        else {
            (*cols)++;
        }
    }
}


size_t find_next_newline(const char *str) {
    for (size_t i = 0;; i++) {
        if (str[i] == '\n' || str[i] == '\0') {
            return i;
        }
    }
}


void print_nth_line(FILE *stream, const char *source, size_t line) {
    for (size_t i = 1; i <= line; i++) {
        size_t line_index = find_next_newline(source);
        if (source[line_index] == '\0') break;
        source += line_index + 1;
    }

    size_t end_index = find_next_newline(source);
    for (size_t i = 0; i < end_index; i++) {
        fprintf(stream, "%c", source[i]);
    }
}


int lexer_init(Lexer *lexer, const char *source) {
    lexer->source = source;
    lexer->id = 0;
    lexer->source_length = strlen(source);
    lexer->index = 0;
    lexer->prev_index = 0;
    lexer->line_num = 0;
    lexer->col_num = 0;

    for (size_t i = 0; i < TOKEN_COUNT; i++) {
        regex_t expression;
        int result = regcomp(&expression, RAW_TOKEN_EXPRESSIONS[i], REG_EXTENDED);
        if (result) {
            fprintf(stderr, "Failed to parse token expression at index %d\n", i);
            return 1;
        }
        lexer->token_expressions[i] = expression;
    }

    return 0;
}


int lexer_next_helper(Token *token, Lexer *lexer) {
    if (lexer->index >= lexer->source_length) {
        return 1;
    }

    const char *str = lexer->source + lexer->index;
    for (TokenKind kind = 0; kind < TOKEN_COUNT; kind++) {
        regex_t *expression = &lexer->token_expressions[kind];
        regmatch_t match;
        int success = regexec(expression, str, 1, &match, 0);
        
        if (success == 0) {
            token->source = lexer->source;
            token->lexer_id = lexer->id;
            token->index = lexer->index;
            token->length = match.rm_eo;
            token->kind = kind;
            token->line_num = lexer->line_num;
            token->col_num = lexer->col_num;
            
            lexer->prev_index = lexer->index;
            lexer->index += token->length;

            size_t lines, cols;
            count_lines_cols(&lines, &cols, token->source+token->index, token->length);

            lexer->line_num += lines;
            lexer->col_num += cols;
            if (lines > 0) {
                lexer->col_num = cols;
            }

            return 0;
        }
    }

    return -1;  // Unrecognized token
}


int lexer_next(Token *token, Lexer *lexer) {
    do {
        int result = lexer_next_helper(token, lexer);
        if (result) {
            return result;
        }
    } while (token->kind == WHITESPACE || token->kind == COMMENT);

    return 0;
}


int lexer_prev(Lexer *lexer) {
    lexer->index = lexer->prev_index;
}


Token lexer_peek(Lexer *lexer) {
    Token token;
    lexer_next(&token, lexer);
    lexer_prev(lexer);
    return token;
}


int lexer_is_done(const Lexer *lexer) {
    return lexer->index >= lexer->source_length;
}


char* get_token_string(Token token) {
    const char *source = token.source + token.index;

    char *result = malloc(sizeof(char) * (token.length + 1));
    strncpy(result, source, token.length);
    result[token.length] = '\0';

    return result;
}


void print_token(Token token) {
    char token_value[128];
    strncpy(token_value, token.source+token.index, token.length);
    token_value[token.length] = '\0';

    printf("Token(kind: %s, value: \"%s\", pos: (%d, %d))\n", TOKEN_KINDS[token.kind], token_value, token.line_num, token.col_num);
}


void print_token_error(Token token, const char *message) {
    fprintf(stderr, "%sERROR: %s\n", COL_ERROR, message);
    fprintf(stderr, "%d | ", token.line_num+1);
    print_nth_line(stderr, token.source, token.line_num);
    fprintf(stderr, "\n");

    for (size_t i = 1; i <= token.col_num+4; i++) {
        fprintf(stderr, " ");
    }
    fprintf(stderr, "^\n%s", COL_RESET);

}
