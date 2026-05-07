/*
 * ShuntingYard.h  —  Infix-to-postfix (RPN) converter for WHERE expressions.
 *
 * Implements the Shunting-Yard algorithm (Dijkstra, 1961).
 *
 * Input:  flat Token array from Tokenizer (infix order).
 * Output: Token array in Reverse Polish Notation (postfix).
 *
 * Viva Q: "Why convert to postfix before evaluation?"
 *   Postfix eliminates the need for parentheses and precedence checks during
 *   evaluation — a simple stack-based evaluator can process it left-to-right
 *   in a single pass in O(N).  Evaluating infix directly requires recursive
 *   descent or tracking precedence at eval time, which is more complex.
 *
 * Viva Q: "What is operator associativity and why does it matter?"
 *   Left-associative: a-b-c means (a-b)-c. For equal-precedence operators,
 *   the stack is drained before pushing the new operator.
 *   Right-associative: NOT NOT x means NOT(NOT(x)). The stack is NOT drained
 *   for equal-precedence — the new operator stacks on top.
 *
 * Operator precedence table (higher = binds tighter):
 *   OR:                   1  (left)
 *   AND:                  2  (left)
 *   NOT:                  3  (right, unary prefix)
 *   >, <, >=, <=, =, !=:  4  (left)
 *   +, -:                 5  (left)
 *   *, /:                 6  (left)
 */
#ifndef SHUNTING_YARD_H
#define SHUNTING_YARD_H

#include "Token.h"
#include "Stack.h"
#include <stdio.h>

class ShuntingYard {
public:
    Token output[MAX_TOKENS];  // postfix result
    int   outCount;            // number of tokens in output

    ShuntingYard();

    /**
     * convert() — Run the Shunting-Yard algorithm.
     * Reads tokens from `in[0..count)`, writes postfix to output[].
     * Stops at TOK_EOF, TOK_SEMICOLON, or count reached.
     *
     * @param in     Token array (infix), typically the WHERE clause portion.
     * @param count  Number of valid tokens in `in`.
     */
    void convert(const Token* in, int count);

    /** Print the postfix token sequence (for debugging). */
    void dump() const;

private:
    /** Operator precedence. Returns 0 for non-operators. */
    static int  precedence(TokenType t);

    /** True if the token is a left-associative binary operator. */
    static bool isLeftAssoc(TokenType t);

    /** True if the token is any operator (binary or unary). */
    static bool isOperator(TokenType t);
};

#endif // SHUNTING_YARD_H
