/*
 * Tokenizer.cpp  —  SQL lexer implementation.
 */
#include "Tokenizer.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

// ── Helpers ───────────────────────────────────────────────────────────────────
bool Tokenizer::isIdentStart(char c) { return isalpha(c) || c == '_'; }
bool Tokenizer::isIdentChar(char c)  { return isalnum(c) || c == '_'; }

Tokenizer::Tokenizer() : count(0), src(0), pos(0) {}

void Tokenizer::addToken(TokenType t, const char* lex, int iv, double dv) {
    if (count >= MAX_TOKENS - 1) return;
    Token& tk = tokens[count++];
    tk.type = t;
    strncpy(tk.lexeme, lex, MAX_LEXEME - 1);
    tk.lexeme[MAX_LEXEME - 1] = '\0';
    tk.ival = iv;
    tk.dval = dv;
}

void Tokenizer::skipWhitespace() {
    while (src[pos] != '\0' && isspace(src[pos])) pos++;
}

// ── Keyword classifier (case-insensitive) ─────────────────────────────────────
// Converts ident to uppercase for comparison.
TokenType Tokenizer::classify(const char* ident) const {
    // Build uppercase version
    char up[MAX_LEXEME];
    int i = 0;
    while (ident[i] && i < MAX_LEXEME - 1) {
        up[i] = (char)toupper((unsigned char)ident[i]);
        i++;
    }
    up[i] = '\0';

    if (strcmp(up, "SELECT")  == 0) return TOK_SELECT;
    if (strcmp(up, "FROM")    == 0) return TOK_FROM;
    if (strcmp(up, "WHERE")   == 0) return TOK_WHERE;
    if (strcmp(up, "INSERT")  == 0) return TOK_INSERT;
    if (strcmp(up, "INTO")    == 0) return TOK_INTO;
    if (strcmp(up, "VALUES")  == 0) return TOK_VALUES;
    if (strcmp(up, "CREATE")  == 0) return TOK_CREATE;
    if (strcmp(up, "TABLE")   == 0) return TOK_TABLE;
    if (strcmp(up, "DROP")    == 0) return TOK_DROP;
    if (strcmp(up, "LIST")    == 0) return TOK_LIST;
    if (strcmp(up, "TABLES")  == 0) return TOK_TABLES;
    if (strcmp(up, "AND")     == 0) return TOK_AND;
    if (strcmp(up, "OR")      == 0) return TOK_OR;
    if (strcmp(up, "NOT")     == 0) return TOK_NOT;
    if (strcmp(up, "INT")     == 0) return TOK_INT_KW;
    if (strcmp(up, "FLOAT")   == 0) return TOK_FLOAT_KW;
    if (strcmp(up, "STRING")  == 0) return TOK_STRING_KW;
    return TOK_IDENT;
}

// ── Readers ───────────────────────────────────────────────────────────────────
void Tokenizer::readIdent() {
    char buf[MAX_LEXEME];
    int  len = 0;
    while (src[pos] != '\0' && isIdentChar(src[pos]) && len < MAX_LEXEME - 1) {
        buf[len++] = src[pos++];
    }
    buf[len] = '\0';
    TokenType t = classify(buf);
    addToken(t, buf);
}

void Tokenizer::readNumber() {
    char buf[MAX_LEXEME];
    int  len = 0;
    bool isFloat = false;
    while (src[pos] != '\0' && (isdigit(src[pos]) || src[pos] == '.') && len < MAX_LEXEME - 1) {
        if (src[pos] == '.') isFloat = true;
        buf[len++] = src[pos++];
    }
    buf[len] = '\0';
    if (isFloat) addToken(TOK_FLOAT_LIT, buf, 0, atof(buf));
    else         addToken(TOK_INT_LIT,   buf, atoi(buf), 0.0);
}

void Tokenizer::readString() {
    pos++;  // skip opening quote
    char buf[MAX_LEXEME];
    int  len = 0;
    while (src[pos] != '\0' && src[pos] != '\'' && len < MAX_LEXEME - 1) {
        buf[len++] = src[pos++];
    }
    if (src[pos] == '\'') pos++;  // skip closing quote
    buf[len] = '\0';
    addToken(TOK_STR_LIT, buf);
}

void Tokenizer::readOperator() {
    char c = src[pos++];
    char next = src[pos];
    switch (c) {
        case '*': addToken(TOK_STAR,      "*");  break;
        case '+': addToken(TOK_PLUS,      "+");  break;
        case '-': addToken(TOK_MINUS,     "-");  break;
        case '/': addToken(TOK_SLASH,     "/");  break;
        case '(': addToken(TOK_LPAREN,    "(");  break;
        case ')': addToken(TOK_RPAREN,    ")");  break;
        case ',': addToken(TOK_COMMA,     ",");  break;
        case ';': addToken(TOK_SEMICOLON, ";");  break;
        case '=': addToken(TOK_EQ,        "=");  break;
        case '>':
            if (next == '=') { pos++; addToken(TOK_GTE, ">="); }
            else               addToken(TOK_GT,  ">");
            break;
        case '<':
            if (next == '=') { pos++; addToken(TOK_LTE,  "<="); }
            else if (next == '>') { pos++; addToken(TOK_NEQ, "<>"); }
            else  addToken(TOK_LT, "<");
            break;
        case '!':
            if (next == '=') { pos++; addToken(TOK_NEQ, "!="); }
            else addToken(TOK_UNKNOWN, "!");
            break;
        default: {
            char buf[2] = { c, '\0' };
            addToken(TOK_UNKNOWN, buf);
        }
    }
}

// ── Main entry point ──────────────────────────────────────────────────────────
void Tokenizer::tokenize(const char* sql) {
    src   = sql;
    pos   = 0;
    count = 0;

    while (src[pos] != '\0') {
        skipWhitespace();
        if (src[pos] == '\0') break;

        char c = src[pos];
        if (isIdentStart(c))   readIdent();
        else if (isdigit(c))   readNumber();
        else if (c == '\'')    readString();
        else                   readOperator();
    }
    addToken(TOK_EOF, "");
}

// ── Debug dump ────────────────────────────────────────────────────────────────
static const char* tokenTypeName(TokenType t) {
    switch (t) {
        case TOK_SELECT:    return "SELECT";
        case TOK_FROM:      return "FROM";
        case TOK_WHERE:     return "WHERE";
        case TOK_INSERT:    return "INSERT";
        case TOK_INTO:      return "INTO";
        case TOK_VALUES:    return "VALUES";
        case TOK_CREATE:    return "CREATE";
        case TOK_TABLE:     return "TABLE";
        case TOK_DROP:      return "DROP";
        case TOK_LIST:      return "LIST";
        case TOK_TABLES:    return "TABLES";
        case TOK_AND:       return "AND";
        case TOK_OR:        return "OR";
        case TOK_NOT:       return "NOT";
        case TOK_INT_KW:    return "INT_KW";
        case TOK_FLOAT_KW:  return "FLOAT_KW";
        case TOK_STRING_KW: return "STRING_KW";
        case TOK_INT_LIT:   return "INT_LIT";
        case TOK_FLOAT_LIT: return "FLOAT_LIT";
        case TOK_STR_LIT:   return "STR_LIT";
        case TOK_IDENT:     return "IDENT";
        case TOK_STAR:      return "STAR";
        case TOK_PLUS:      return "PLUS";
        case TOK_MINUS:     return "MINUS";
        case TOK_SLASH:     return "SLASH";
        case TOK_GT:        return "GT";
        case TOK_LT:        return "LT";
        case TOK_GTE:       return "GTE";
        case TOK_LTE:       return "LTE";
        case TOK_EQ:        return "EQ";
        case TOK_NEQ:       return "NEQ";
        case TOK_LPAREN:    return "LPAREN";
        case TOK_RPAREN:    return "RPAREN";
        case TOK_COMMA:     return "COMMA";
        case TOK_SEMICOLON: return "SEMICOLON";
        case TOK_EOF:       return "EOF";
        default:            return "UNKNOWN";
    }
}

void Tokenizer::dump() const {
    printf("[Tokenizer] %d tokens:\n", count);
    for (int i = 0; i < count; i++) {
        printf("  [%2d] %-12s  '%s'\n", i,
               tokenTypeName(tokens[i].type), tokens[i].lexeme);
    }
}
