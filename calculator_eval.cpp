// src/tools/calculator_eval.cpp — calc_evaluate (spec §4.6).
//
// Two-stack evaluation: `values` holds numbers, `ops` holds operators and '('.
// Loops only (no recursion), so 20,000 terms or 5,000 nested parentheses are fine.

#include "tools/calculator.h"

#include <string>
#include <vector>

namespace {

using Kind = CalcToken::Kind;

bool is_operator(Kind k) {
    return k == Kind::Plus || k == Kind::Minus || k == Kind::Times || k == Kind::Divide;
}

int precedence(Kind k) {
    return (k == Kind::Times || k == Kind::Divide) ? 2 : 1;
}

// Pop one operator and two operands, push the result.
// The first value popped is the RIGHT operand.
bool apply_top(std::vector<double>& values, std::vector<Kind>& ops, std::string* error) {
    if (ops.empty() || values.size() < 2) {
        *error = "malformed expression";
        return false;
    }
    Kind op = ops.back();
    ops.pop_back();
    double right = values.back();
    values.pop_back();
    double left = values.back();
    values.pop_back();

    double result = 0.0;
    switch (op) {
        case Kind::Plus:  result = left + right; break;
        case Kind::Minus: result = left - right; break;
        case Kind::Times: result = left * right; break;
        case Kind::Divide:
            if (right == 0.0) {
                *error = "division by zero";
                return false;
            }
            result = left / right;
            break;
        default:
            *error = "malformed expression";
            return false;
    }
    values.push_back(result);
    return true;
}

}  // namespace

bool calc_evaluate(const std::vector<CalcToken>& tokens, double* out, std::string* error) {
    std::vector<double> values;
    std::vector<Kind> ops;

    for (const CalcToken& t : tokens) {
        switch (t.kind) {
            case Kind::Number:
                values.push_back(t.value);
                break;
            case Kind::LParen:
                ops.push_back(Kind::LParen);
                break;
            case Kind::RParen:
                while (!ops.empty() && ops.back() != Kind::LParen) {
                    if (!apply_top(values, ops, error)) return false;
                }
                if (ops.empty()) {
                    *error = "unbalanced parentheses";
                    return false;
                }
                ops.pop_back();  // discard the '('
                break;
            default:  // + - * /
                // >= (not >) is what makes 10 - 4 - 3 evaluate left to right.
                while (!ops.empty() && is_operator(ops.back()) &&
                       precedence(ops.back()) >= precedence(t.kind)) {
                    if (!apply_top(values, ops, error)) return false;
                }
                ops.push_back(t.kind);
                break;
        }
    }

    while (!ops.empty()) {
        if (ops.back() == Kind::LParen) {
            *error = "unbalanced parentheses";
            return false;
        }
        if (!apply_top(values, ops, error)) return false;
    }

    if (values.size() != 1) {
        *error = "malformed expression";
        return false;
    }
    *out = values.back();
    return true;
}
