#pragma once

#include "../../ast/ast.hpp"
#include "../../utils.hpp"
#include "x86prog.hpp"
#include "x86Instructions.hpp"

class x86Generator {

    static void eval(x86Prog& p, std::unique_ptr<Expression> expr) {

        const auto visitor = overloads {
            [&](Term& t) {
                p.push_value(static_cast<uint32_t>(std::get<long long>(t.v)));
            },
            [&](Add& e) {
                eval(p, std::move(e.left));
                eval(p, std::move(e.right));
                p.add();
            },
            [&](Sub& e) {
                eval(p, std::move(e.left));
                eval(p, std::move(e.right));
                p.sub();
            },
            [&](Mult& e) {
                eval(p, std::move(e.left));
                eval(p, std::move(e.right));
                p.mult();
            },
            [&](Div& e) {
                eval(p, std::move(e.left));
                eval(p, std::move(e.right));
                p.div();
            },
            [&](Mod& e) {
                eval(p, std::move(e.left));
                eval(p, std::move(e.right));
                p.mod();
            },
            [&](Exp& e) {
                eval(p, std::move(e.base));
                eval(p, std::move(e.exponent));
                p.exp();
            },
            [&](Negated& e) {
                eval(p, std::move(e.expression));
                p.neg();
            },
            [&](FunctionCall& v) {
                //<needs implementing>;
            },
            [&](std::monostate) {}
        };

        std::visit(visitor, expr->expression);
    }

public:

    static x86Prog generate(std::unique_ptr<Expression> expr) {
        x86Prog p;
        eval(p, std::move(expr));
        p.prog_fn = x86::compose(
            p.prog_fn,
            x86::pop(x86::Register::EAX),
            x86::ret()
        );
        return p;
    }
};