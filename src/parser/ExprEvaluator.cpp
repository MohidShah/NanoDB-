/*
 * ExprEvaluator.cpp  —  Postfix expression evaluator.
 */
#include "ExprEvaluator.h"
#include <stdio.h>
#include <string.h>

// ── compare() ─────────────────────────────────────────────────────────────────
EvalValue ExprEvaluator::compare(const EvalValue& a, const EvalValue& b, TokenType op) {
    bool result = false;

    if (a.type == EvalValue::STRING_T || b.type == EvalValue::STRING_T) {
        // String comparison via strcmp
        int cmp = strcmp(a.sval, b.sval);
        switch (op) {
            case TOK_EQ:  result = (cmp == 0); break;
            case TOK_NEQ: result = (cmp != 0); break;
            case TOK_GT:  result = (cmp >  0); break;
            case TOK_LT:  result = (cmp <  0); break;
            case TOK_GTE: result = (cmp >= 0); break;
            case TOK_LTE: result = (cmp <= 0); break;
            default:      result = false;
        }
    } else {
        // Numeric comparison — widen to double
        double da = a.toDouble();
        double db = b.toDouble();
        switch (op) {
            case TOK_EQ:  result = (da == db); break;
            case TOK_NEQ: result = (da != db); break;
            case TOK_GT:  result = (da >  db); break;
            case TOK_LT:  result = (da <  db); break;
            case TOK_GTE: result = (da >= db); break;
            case TOK_LTE: result = (da <= db); break;
            default:      result = false;
        }
    }
    return EvalValue(result);
}

// ── arithmetic() ──────────────────────────────────────────────────────────────
EvalValue ExprEvaluator::arithmetic(const EvalValue& a, const EvalValue& b, TokenType op) {
    // Perform in double; if both were INT, return INT.
    double da = a.toDouble();
    double db = b.toDouble();
    double res = 0.0;
    switch (op) {
        case TOK_PLUS:  res = da + db; break;
        case TOK_MINUS: res = da - db; break;
        case TOK_STAR:  res = da * db; break;
        case TOK_SLASH: res = (db != 0.0) ? da / db : 0.0; break;
        default: break;
    }
    if (a.type == EvalValue::INT_T && b.type == EvalValue::INT_T)
        return EvalValue((int)res);
    return EvalValue(res);
}

// ── evaluate() ────────────────────────────────────────────────────────────────
bool ExprEvaluator::evaluate(const Token* postfix, int tokenCount,
                              const Row* row, const Schema* schema,
                              bool* outError) {
    if (outError) *outError = false;

    Stack<EvalValue> stack;

    for (int i = 0; i < tokenCount; i++) {
        const Token& t = postfix[i];

        // ── Push values ───────────────────────────────────────────────────────
        if (t.type == TOK_INT_LIT) {
            stack.push(EvalValue(t.ival));
            continue;
        }
        if (t.type == TOK_FLOAT_LIT) {
            stack.push(EvalValue(t.dval));
            continue;
        }
        if (t.type == TOK_STR_LIT) {
            stack.push(EvalValue(t.lexeme));
            continue;
        }
        if (t.type == TOK_IDENT) {
            // Resolve column name → Field value from the row
            int col = schema->indexOf(t.lexeme);
            if (col < 0) {
                fprintf(stderr, "[ExprEval] unknown column: '%s'\n", t.lexeme);
                if (outError) *outError = true;
                return false;
            }
            const Field* f = row->cols[col];
            switch (f->getType()) {
                case Field::INT: {
                    stack.push(EvalValue((int)f->toDouble()));
                    break;
                }
                case Field::FLOAT: {
                    stack.push(EvalValue(f->toDouble()));
                    break;
                }
                case Field::STRING: {
                    char buf[256];
                    f->toString(buf, 256);
                    stack.push(EvalValue(buf));
                    break;
                }
            }
            continue;
        }

        // ── Logical operators (unary / binary) ────────────────────────────────
        if (t.type == TOK_NOT) {
            if (stack.empty()) { if (outError) *outError = true; return false; }
            EvalValue v = stack.pop();
            stack.push(EvalValue(!v.bval));
            continue;
        }
        if (t.type == TOK_AND) {
            if (stack.size() < 2) { if (outError) *outError = true; return false; }
            EvalValue b = stack.pop();
            EvalValue a = stack.pop();
            stack.push(EvalValue(a.bval && b.bval));
            continue;
        }
        if (t.type == TOK_OR) {
            if (stack.size() < 2) { if (outError) *outError = true; return false; }
            EvalValue b = stack.pop();
            EvalValue a = stack.pop();
            stack.push(EvalValue(a.bval || b.bval));
            continue;
        }

        // ── Comparison operators ──────────────────────────────────────────────
        if (t.type == TOK_EQ  || t.type == TOK_NEQ ||
            t.type == TOK_GT  || t.type == TOK_LT  ||
            t.type == TOK_GTE || t.type == TOK_LTE) {
            if (stack.size() < 2) { if (outError) *outError = true; return false; }
            EvalValue b = stack.pop();
            EvalValue a = stack.pop();
            stack.push(compare(a, b, t.type));
            continue;
        }

        // ── Arithmetic operators ──────────────────────────────────────────────
        if (t.type == TOK_PLUS  || t.type == TOK_MINUS ||
            t.type == TOK_STAR  || t.type == TOK_SLASH) {
            if (stack.size() < 2) { if (outError) *outError = true; return false; }
            EvalValue b = stack.pop();
            EvalValue a = stack.pop();
            stack.push(arithmetic(a, b, t.type));
            continue;
        }
    }

    // ── Result: top of stack should be a BOOL ─────────────────────────────────
    if (stack.empty()) return true;  // empty WHERE = match everything
    EvalValue result = stack.pop();
    if (result.type == EvalValue::BOOL_T)  return result.bval;
    if (result.type == EvalValue::INT_T)   return result.ival != 0;
    return false;
}
