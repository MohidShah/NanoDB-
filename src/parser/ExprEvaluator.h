/*
 * ExprEvaluator.h  —  Evaluates postfix (RPN) expressions against a table Row.
 *
 * Takes the output of ShuntingYard and a Row* + Schema*, and returns true/false
 * for use as a WHERE-clause filter.
 *
 * Viva Q: "How does the evaluator resolve a column name like 'l_quantity'?"
 *   1. It calls schema->indexOf("l_quantity") to get the column index (O(N_cols)).
 *   2. It calls row->fields[colIndex]->toDouble() or reads the Field* directly.
 *   3. It pushes the field's value as an EvalValue onto the evaluation stack.
 *
 * Viva Q: "How are mixed INT/FLOAT comparisons handled?"
 *   Both operands are widened to double before comparison.  This mirrors SQL
 *   standard numeric promotion rules and avoids integer truncation errors.
 *
 * Evaluation stack entry (EvalValue):
 *   A tagged union holding one of: INT, FLOAT, STRING, or BOOL.
 *   String values are needed for comparing STRING fields.
 */
#ifndef EXPR_EVALUATOR_H
#define EXPR_EVALUATOR_H

#include "Token.h"
#include "Stack.h"
#include "../schema/Row.h"
#include "../schema/Schema.h"
#include <string.h>  // strcmp
#include <stdio.h>   // fprintf

// ── Tagged value on the evaluation stack ──────────────────────────────────────
struct EvalValue {
    enum Type { INT_T, FLOAT_T, STRING_T, BOOL_T } type;
    int    ival;
    double dval;
    bool   bval;
    char   sval[256];   // for STRING_T values

    EvalValue()                 : type(BOOL_T),   ival(0),  dval(0.0), bval(false) { sval[0]='\0'; }
    EvalValue(int v)            : type(INT_T),     ival(v),  dval((double)v), bval(false) { sval[0]='\0'; }
    EvalValue(double v)         : type(FLOAT_T),   ival(0),  dval(v),  bval(false) { sval[0]='\0'; }
    EvalValue(bool v)           : type(BOOL_T),    ival(0),  dval(0.0), bval(v)   { sval[0]='\0'; }
    EvalValue(const char* s)    : type(STRING_T),  ival(0),  dval(0.0), bval(false) {
        strncpy(sval, s, 255); sval[255] = '\0';
    }

    // Promote to double for numeric comparisons.
    double toDouble() const { return (type == INT_T) ? (double)ival : dval; }
};

class ExprEvaluator {
public:
    /**
     * evaluate() — Evaluate a postfix expression against a row.
     * @param postfix     Token array in postfix order (output of ShuntingYard).
     * @param tokenCount  Number of valid tokens in postfix.
     * @param row         The row to evaluate the expression against.
     * @param schema      Schema used to resolve column names to indices.
     * @param outError    Set to true if evaluation encountered an error.
     * @return true if the expression is satisfied for this row.
     */
    bool evaluate(const Token* postfix, int tokenCount,
                  const Row* row, const Schema* schema,
                  bool* outError = 0);

private:
    // Compare two EvalValues with operator type tok.
    // Returns a BOOL_T EvalValue.
    static EvalValue compare(const EvalValue& a, const EvalValue& b, TokenType op);

    // Arithmetic on two numeric EvalValues.
    static EvalValue arithmetic(const EvalValue& a, const EvalValue& b, TokenType op);
};

#endif // EXPR_EVALUATOR_H
