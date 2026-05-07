/*
 * Tokenizer.h  —  SQL lexer for NanoDB.
 *
 * Converts a raw SQL string into a flat array of Token objects.
 * Supports a subset of SQL sufficient for NanoDB test cases A-G:
 *   SELECT, INSERT INTO, CREATE TABLE, DROP TABLE, LIST TABLES,
 *   WHERE clauses with comparisons, AND/OR/NOT, parentheses.
 *
 * Viva Q: "What is the difference between a token and a lexeme?"
 *   A lexeme is the raw character sequence from the source (e.g. "SELECT").
 *   A token is the classified pair (TokenType::TOK_SELECT, lexeme="SELECT").
 *   The tokenizer performs lexical analysis — turning bytes into tokens —
 *   before the parser gives them grammatical meaning.
 */
#ifndef TOKENIZER_H
#define TOKENIZER_H

#include "Token.h"
#include <stdio.h>   // printf
#include <string.h>  // strncpy, strcmp, strlen
#include <stdlib.h>  // atoi, atof
#include <ctype.h>   // isalpha, isdigit, isspace

class Tokenizer {
public:
    Token tokens[MAX_TOKENS];  // output token array
    int   count;               // number of tokens produced

    Tokenizer();

    /**
     * tokenize() — Lex the SQL string and populate tokens[].
     * Always appends a TOK_EOF sentinel at the end.
     * @param sql  Null-terminated SQL statement.
     */
    void tokenize(const char* sql);

    /** Print all tokens to stdout (for debugging). */
    void dump() const;

private:
    const char* src;  // current SQL string being scanned
    int         pos;  // current scan position in src

    void skipWhitespace();
    void readIdent();       // reads [a-zA-Z_][a-zA-Z0-9_]*
    void readNumber();      // reads integer or float literal
    void readString();      // reads 'single-quoted string'
    void readOperator();    // reads punctuation & multi-char operators

    /** Classify a raw identifier string as keyword or TOK_IDENT. */
    TokenType classify(const char* ident) const;

    /** Append a fully-built token to tokens[]. */
    void addToken(TokenType t, const char* lexeme, int iv = 0, double dv = 0.0);

    static bool isIdentStart(char c);
    static bool isIdentChar(char c);
};

#endif // TOKENIZER_H
