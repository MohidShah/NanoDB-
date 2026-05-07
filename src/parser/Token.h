/*
 * Token.h  —  SQL token definitions for NanoDB parser.
 *
 */
#ifndef TOKEN_H
#define TOKEN_H

static const int MAX_LEXEME = 256;   // max raw token text length
static const int MAX_TOKENS = 512;   // max tokens per SQL statement

enum TokenType {
    // ── SQL Keywords ──────────────────────────────────────────────────────────
    TOK_SELECT, TOK_FROM, TOK_WHERE,
    TOK_INSERT, TOK_INTO, TOK_VALUES,
    TOK_CREATE, TOK_TABLE, TOK_DROP,
    TOK_LIST,   TOK_TABLES,
    TOK_AND,    TOK_OR,   TOK_NOT,
    TOK_INT_KW, TOK_FLOAT_KW, TOK_STRING_KW,  // column type keywords

    // ── Literals & identifiers ────────────────────────────────────────────────
    TOK_INT_LIT,    // e.g. 42
    TOK_FLOAT_LIT,  // e.g. 3.14
    TOK_STR_LIT,    // e.g. 'hello'  (lexeme stored without quotes)
    TOK_IDENT,      // column / table name

    // ── Operators ─────────────────────────────────────────────────────────────
    TOK_STAR,   // * (SELECT * or multiply)
    TOK_PLUS,   // +
    TOK_MINUS,  // -
    TOK_SLASH,  // /
    TOK_GT,     // >
    TOK_LT,     // <
    TOK_GTE,    // >=
    TOK_LTE,    // <=
    TOK_EQ,     // =
    TOK_NEQ,    // != or <>

    // ── Punctuation ───────────────────────────────────────────────────────────
    TOK_LPAREN, TOK_RPAREN, TOK_COMMA, TOK_SEMICOLON,

    TOK_EOF,
    TOK_UNKNOWN
};

struct Token {
    TokenType type;
    char      lexeme[MAX_LEXEME];  // raw text as-is from SQL input
    int       ival;                // populated for TOK_INT_LIT
    double    dval;                // populated for TOK_FLOAT_LIT

    Token() : type(TOK_UNKNOWN), ival(0), dval(0.0) {
        lexeme[0] = '\0';
    }
};

#endif // TOKEN_H
