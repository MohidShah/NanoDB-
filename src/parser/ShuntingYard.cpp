/*
 * ShuntingYard.cpp  —  Infix-to-postfix implementation.
 */
#include "ShuntingYard.h"
#include <stdio.h>

ShuntingYard::ShuntingYard() : outCount(0) {}

// ── Precedence table ──────────────────────────────────────────────────────────
int ShuntingYard::precedence(TokenType t) {
    switch (t) {
        case TOK_OR:    return 1;
        case TOK_AND:   return 2;
        case TOK_NOT:   return 3;  // unary prefix, right-associative
        case TOK_EQ:
        case TOK_NEQ:
        case TOK_GT:
        case TOK_LT:
        case TOK_GTE:
        case TOK_LTE:   return 4;
        case TOK_PLUS:
        case TOK_MINUS: return 5;
        case TOK_STAR:
        case TOK_SLASH: return 6;
        default:        return 0;
    }
}

bool ShuntingYard::isLeftAssoc(TokenType t) {
    // Only NOT is right-associative; all other operators are left.
    return t != TOK_NOT;
}

bool ShuntingYard::isOperator(TokenType t) {
    return precedence(t) > 0;
}

// ── Shunting-Yard algorithm ───────────────────────────────────────────────────
void ShuntingYard::convert(const Token* in, int count) {
    outCount = 0;
    Stack<Token> opStack;

    for (int i = 0; i < count; i++) {
        const Token& t = in[i];

        // ── Stop conditions ───────────────────────────────────────────────────
        if (t.type == TOK_EOF || t.type == TOK_SEMICOLON) break;

        // ── Values: push directly to output ──────────────────────────────────
        if (t.type == TOK_IDENT    ||
            t.type == TOK_INT_LIT  ||
            t.type == TOK_FLOAT_LIT||
            t.type == TOK_STR_LIT) {
            output[outCount++] = t;
            continue;
        }

        // ── Operator ──────────────────────────────────────────────────────────
        if (isOperator(t.type)) {
            while (!opStack.empty()) {
                TokenType top = opStack.peek().type;
                if (top == TOK_LPAREN) break;
                if (!isOperator(top))  break;
                // Pop if top has higher precedence, OR same + left-assoc.
                int topPrec = precedence(top);
                int curPrec = precedence(t.type);
                if (topPrec > curPrec || (topPrec == curPrec && isLeftAssoc(t.type))) {
                    output[outCount++] = opStack.pop();
                } else {
                    break;
                }
            }
            opStack.push(t);
            continue;
        }

        // ── Left parenthesis: push to operator stack ──────────────────────────
        if (t.type == TOK_LPAREN) {
            opStack.push(t);
            continue;
        }

        // ── Right parenthesis: pop to output until matching LPAREN ────────────
        if (t.type == TOK_RPAREN) {
            while (!opStack.empty() && opStack.peek().type != TOK_LPAREN) {
                output[outCount++] = opStack.pop();
            }
            if (!opStack.empty()) opStack.pop();  // discard LPAREN
            continue;
        }
        // Commas, keywords, punctuation: skip (not part of expression)
    }

    // ── Drain remaining operators ─────────────────────────────────────────────
    while (!opStack.empty()) {
        Token top = opStack.pop();
        if (top.type != TOK_LPAREN) {  // unmatched parens: skip
            output[outCount++] = top;
        }
    }
}

// ── Debug dump ────────────────────────────────────────────────────────────────
void ShuntingYard::dump() const {
    printf("[ShuntingYard] postfix (%d tokens): ", outCount);
    for (int i = 0; i < outCount; i++) {
        printf("%s ", output[i].lexeme[0] ? output[i].lexeme : "?");
    }
    printf("\n");
}
