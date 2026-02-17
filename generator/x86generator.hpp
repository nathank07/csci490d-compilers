#pragma once

#include "../ast/ast.hpp"
#include "x86operands.hpp"
#include "x86prog.hpp"
#include <functional>
#include <memory>

class x86Generator {

    static auto put_stack(long long term) {
        return [term](struct x86Prog& p) {
            p.mov(Register::EAX, Immediate{term});
            p.push(Register::EAX);
        };
    }

    static auto handle_add() {
        return [](struct x86Prog& p) {
            p.pop(Register::ECX); 
            p.pop(Register::EAX);
            p.add(Register::EAX, Register::ECX);
            p.push(Register::EAX);
        };
    }

    static auto handle_sub() {
        return [](struct x86Prog& p) {
            p.pop(Register::ECX);
            p.pop(Register::EAX);
            p.sub(Register::EAX, Register::ECX);
            p.push(Register::EAX);
        };
    }

    static auto handle_mult() {
        return [](struct x86Prog& p) {
            
        };
    }

    static auto handle_div() {
        return [](struct x86Prog& p) {

        };
    }

    static auto handle_mod() {
        return [](struct x86Prog& p) {

        };
    }

    static auto handle_exp() {
        return [](struct x86Prog& p) {

        };
    }

public:

    using Instructions = std::function<void(x86Prog&)>;

    static auto compose(auto f1, auto f2) {
        return [f1, f2](struct x86Prog& p) {
            f1(p);
            f2(p);
        };
    }
    
    static Instructions evaluate_expression(std::unique_ptr<Expression> expr) {
        const auto visitor = overloads {
            [&](Term& t) -> Instructions {
                return put_stack(std::get<long long>(t.v));
            },
            [&](Add& e) -> Instructions {
                return compose(
                    compose(
                        evaluate_expression(std::move(e.left)),
                        evaluate_expression(std::move(e.right))
                    ),
                    handle_add()
                );
            },
            [&](Sub& e) -> Instructions {
                return compose(
                    compose(
                        evaluate_expression(std::move(e.left)),
                        evaluate_expression(std::move(e.right))
                    ),
                    handle_sub()
                );
            },
            [&](Mult& e) -> Instructions {
                return compose(
                    compose(
                        evaluate_expression(std::move(e.left)),
                        evaluate_expression(std::move(e.right))
                    ),
                    handle_mult()
                );
            },
            [&](Div& e) -> Instructions {
                return compose(
                    compose(
                        evaluate_expression(std::move(e.left)),
                        evaluate_expression(std::move(e.right))
                    ),
                    handle_div()
                );
            },
            [&](Mod& e) -> Instructions {
                return compose(
                    compose(
                        evaluate_expression(std::move(e.left)),
                        evaluate_expression(std::move(e.right))
                    ),
                    handle_mod()
                );
            },
            [&](Exp& e) -> Instructions {
                return compose(
                    compose(
                        evaluate_expression(std::move(e.base)),
                        evaluate_expression(std::move(e.exponent))
                    ),
                    handle_exp()
                );
            },
            [&](Negated& e) -> Instructions {
                return evaluate_expression(std::move(e.expression));
            },
            [&](std::monostate) -> Instructions {
                return [](x86Prog&) {};
            }
        };

        return std::visit(visitor, expr->expression);
    }
};