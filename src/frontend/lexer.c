#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <regex.h>
#include "lexer.h"
#include "../globals.h"


const char *RAW_TOKEN_EXPRESSIONS[] = {
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

const char *IGNORED_EXPRESSIONS[] = {
    "^[[:space:]]+",
    "^\\/\\/[^\n]*"
};


void update_lexer_pos(Lexer *lexer, size_t length) {
    const char *str = lexer->source + lexer->index;

    for (size_t i = 0; i < length; i++) {
        if (str[i] == '\n') {
            lexer->line_num++;
            lexer->col_num = 0;
        }
        else {
            lexer->col_num++;
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


void print_token(Token token) {
    char token_value[128];
    strncpy(token_value, token.source+token.index, token.length);
    token_value[token.length] = '\0';

    printf("Token(kind: %s, value: \"%s\", pos: (%ld, %ld))\n", TOKEN_KINDS[token.kind], token_value, token.line_num, token.col_num);
}


void print_token_error(Token token, const char *message) {
    fprintf(stderr, "%sERROR: %s\n", COL_ERROR, message);
    fprintf(stderr, "%ld | ", token.line_num+1);
    print_nth_line(stderr, token.source, token.line_num);
    fprintf(stderr, "\n");

    for (size_t i = 1; i <= token.col_num+4; i++) {
        fprintf(stderr, " ");
    }
    fprintf(stderr, "^\n%s", COL_RESET);
}



int lexer_init(Lexer *lexer, const char *source) {
    lexer->source = source;
    lexer->id = 0;
    lexer->source_length = strlen(source);
    lexer->index = 0;
    lexer->prev_index = 0;
    lexer->line_num = 0;
    lexer->col_num = 0;

    // Compile token expressions
    for (size_t i = 0; i < TOKEN_COUNT; i++) {
        regex_t expression;
        int result = regcomp(&expression, RAW_TOKEN_EXPRESSIONS[i], REG_EXTENDED);
        if (result) {
            fprintf(stderr, "Failed to parse token expression at index %ld\n", i);
            return 1;
        }
        lexer->token_expressions[i] = expression;
    }

    // Compile ignored expressions
    for (size_t i = 0; i < IGNORED_COUNT; i++) {
        regex_t expression;
        int result = regcomp(&expression, IGNORED_EXPRESSIONS[i], REG_EXTENDED);
        if (result) {
            fprintf(stderr, "Failed to parse ignored expression at index %ld\n", i);
            return 1;
        }
        lexer->ignored_expressions[i] = expression;
    }

    return 0;
}


int lexer_next(Token *token, Lexer *lexer) {
    if (lexer_is_done(lexer)) {
        return 1;
    }
    
    // Check for ignored expressions
    while (1) {
        const char *str = lexer->source + lexer->index;
        int matched = 0;

        for (size_t i = 0; i < IGNORED_COUNT; i++) {
            regex_t *expression = &lexer->ignored_expressions[i];
            regmatch_t match;
            int match_result = regexec(expression, str, 1, &match, 0);

            if (match_result == 0) {
                update_lexer_pos(lexer, match.rm_eo);
                lexer->index += match.rm_eo;
                matched = 1;
                break;
            }
        }

        if (!matched) break;
    }

    if (lexer_is_done(lexer)) {
        return 1;
    }

    const char *str = lexer->source + lexer->index;  // Skip past any ignored characters

    // Try to match a token
    for (TokenKind kind = 0; kind < TOKEN_COUNT; kind++) {
        regex_t *expression = &lexer->token_expressions[kind];
        regmatch_t match;
        int match_result = regexec(expression, str, 1, &match, 0);
        
        if (match_result == 0) {
            token->source = lexer->source;
            token->lexer_id = lexer->id;
            token->index = lexer->index;
            token->length = match.rm_eo;
            token->kind = kind;
            token->line_num = lexer->line_num;
            token->col_num = lexer->col_num;

            update_lexer_pos(lexer, token->length);

            lexer->prev_index = lexer->index;
            lexer->index += token->length;

            return 0;
        }
    }

    return -1;  // Unrecognized token
}


void lexer_prev(Lexer *lexer) {
    lexer->index = lexer->prev_index;
}


Token lexer_peek(Lexer *lexer) {
    Token token = {0};
    if (lexer_next(&token, lexer)) {
        return token;
    }
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