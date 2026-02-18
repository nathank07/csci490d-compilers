#include "x86generator.hpp"

std::function<void(x86Prog&)> x86Generator::eval(std::unique_ptr<Expression> expr) {
    const auto visitor = overloads {
        [&](Term& t) -> std::function<void(x86Prog&)> {
            return put_stack(static_cast<uint32_t>(std::get<long long>(t.v)));
        },
        [&](Add& e) -> std::function<void(x86Prog&)> {
            return compose(
                eval(std::move(e.left)),
                eval(std::move(e.right)),
                handle_add()
            );
        },
        [&](Sub& e) -> std::function<void(x86Prog&)> {
            return compose(
                eval(std::move(e.left)),
                eval(std::move(e.right)),
                handle_sub()
            );
        },
        [&](Mult& e) -> std::function<void(x86Prog&)> {
            return compose(
                eval(std::move(e.left)),
                eval(std::move(e.right)),
                handle_mult()
            );
        },
        [&](Div& e) -> std::function<void(x86Prog&)> {
            return compose(
                eval(std::move(e.left)),
                eval(std::move(e.right)),
                handle_div()
            );
        },
        [&](Mod& e) -> std::function<void(x86Prog&)> {
            return compose(
                eval(std::move(e.left)),
                eval(std::move(e.right)),
                handle_mod()
            );
        },
        [&](Exp& e) -> std::function<void(x86Prog&)> {
            return compose(
                eval(std::move(e.base)),
                eval(std::move(e.exponent)),      
                handle_exp()
            );
        },
        [&](Negated& e) -> std::function<void(x86Prog&)> {
            return compose(
                eval(std::move(e.expression)),
                handle_neg()
            );
        },
        [&](std::monostate) -> std::function<void(x86Prog&)> {
            return [](x86Prog&) {};
        }
    };

    return std::visit(visitor, expr->expression);
}