#pragma once

#include "../ast/ast.hpp"
#include "x86operands.hpp"
#include "x86prog.hpp"
#include <memory>

class x86Generator {

    static auto put_stack(long long term) {
        return [term](x86Prog& p) {
            p.mov(Register::EAX, Immediate{term});
            p.push(Register::EAX);
        };
    }

    static auto handle_add() {
        return [](x86Prog& p) {
            p.pop(Register::EAX);
        };
    }

    static auto handle_sub() {
        return [](x86Prog& p) {

        };
    }

    static auto handle_mult() {
        return [](x86Prog& p) {

        };
    }

    static auto handle_div() {
        return [](x86Prog& p) {

        };
    }

    static auto handle_mod() {
        return [](x86Prog& p) {

        };
    }

    static auto handle_exp() {
        return [](x86Prog& p) {

        };
    }

public:
    static void evaluate_expression(x86Prog& p, std::unique_ptr<Expression> expr) {
        const auto visitor = overloads {
            [&](Term& t) {
                p.add_instructions(put_stack(std::get<long long>(t.v)));
            },
            [&](Add& e) {
                evaluate_expression(p, std::move(e.left));
                evaluate_expression(p, std::move(e.right));
                p.add_instructions(handle_add());
            },
            [&](Sub& e) {

            },
            [&](Mult& e) {

            },
            [&](Div& e) {

            },
            [&](Mod& e) {

            },
            [&](Exp& e) {

            },
            [&](Negated& e) {

            },
            [&](std::monostate) {

            }
        };

        std::visit(visitor, expr->expression);
    }
};