#pragma once

#include "../../ast/ast.hpp"
#include "../../utils.hpp"
#include "midosProg.hpp"

class midosGenerator {

    static void eval(midOsProg& p, std::unique_ptr<Expression> expr) {
        using Register = midOsProg::Register;

        const auto visitor = overloads {
            [&](Term& t) {
                p.push_value(Register::R1, static_cast<uint32_t>(std::get<long long>(t.v)));
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
                // TODO: mod not yet in midOsProg
            },
            [&](Exp& e) {
                eval(p, std::move(e.base));
                eval(p, std::move(e.exponent));
                // TODO: exp not yet in midOsProg
            },
            [&](Negated& e) {
                eval(p, std::move(e.expression));
                p.neg();
            },
            [&](std::monostate) {}
        };

        std::visit(visitor, expr->expression);
    }

public:

    static void generate(midOsProg& p, std::unique_ptr<Expression> expr) {
        eval(p, std::move(expr));
    }
};
