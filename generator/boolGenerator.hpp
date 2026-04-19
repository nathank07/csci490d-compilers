#pragma once
#include "../ast/expression.hpp"
#include "../utils.hpp"
#include "stackAllocator.hpp"
#include <functional>

template <typename Architecture>
struct BoolGenerator {

    struct BooleanChunk {
        using Instruction = typename Architecture::Instruction;
        using Cond = typename Architecture::Conditional;
        std::function<Instruction(std::size_t, std::size_t, Cond)> create_instr;
        Cond cond;
    };

private:

    static BooleanChunk handle_or(auto&& lhs, auto&& rhs) {
        return BooleanChunk{ [lhs, rhs](auto on_match, auto on_fail, auto) {
            auto _rhs = rhs.create_instr(on_match, on_fail, rhs.cond);
            auto _lhs = lhs.create_instr(_rhs.byte_size + on_match, 0, lhs.cond);
            return Architecture::compose(_lhs, _rhs);
        }, rhs.cond };
    }

    static BooleanChunk handle_and(auto&& lhs, auto&& rhs) {
        return BooleanChunk{ [lhs, rhs](auto on_match, auto on_fail, auto) {
            auto _rhs = rhs.create_instr(on_match, on_fail, rhs.cond);
            auto _lhs = lhs.create_instr(0, on_fail + _rhs.byte_size, lhs.cond);
            return Architecture::compose(_lhs, _rhs);
        }, rhs.cond };
    }

    static BooleanChunk error() {
        assert(false && "Expected logical");
        return BooleanChunk{ [](auto, auto, auto) {
            return Architecture::compose();
        }, static_cast<typename Architecture::Conditional>(0) };
    }
public:

    template <typename Generator>
    static BooleanChunk eval(Generator& generator, const Expression& expr) {
        const auto visitor = overloads {
            [&](const NumericComparison&) {
                auto compare = generator.eval(expr);
                
                auto c = 
                    StackUtils::assert_cond<typename Generator::Stack::StackUnit, Generator>
                        (generator.stack.pop());

                return BooleanChunk{[compare](auto on_match, auto on_fail, auto cond) {
                    auto jump_on_match = Architecture::jump_rel(static_cast<int32_t>(on_match));
                    return Architecture::compose(
                        compare,
                        Architecture::jump_rel_when(on_fail + jump_on_match.byte_size, Architecture::invert(cond)),
                        jump_on_match
                    );
                }, c };
            },
            [&](const Or& v) {
                return handle_or(eval(generator, **v.left), eval(generator, **v.right));
            },
            [&](const And& v) {
                return handle_and(eval(generator, **v.left), eval(generator, **v.right));
            },
            [&](const BoolConst& v) {
                auto cond = v.is_true ? Architecture::Conditional::UNCONDITIONALLY :
                    Architecture::Conditional::NEVER;
                return BooleanChunk{[cond](auto on_match, auto on_fail, auto) {
                    auto jump_on_match = Architecture::jump_rel(static_cast<int32_t>(on_match));
                    return Architecture::compose(
                        Architecture::jump_rel_when(on_fail + jump_on_match.byte_size, 
                            Architecture::invert(cond)),
                        jump_on_match
                    );
                }, cond};
            },
            [&](const Not& not_v) {
                auto invert = [](BooleanChunk chunk) -> BooleanChunk {
                    return BooleanChunk{chunk.create_instr, Architecture::invert(chunk.cond)};
                };
                const auto not_visitor = overloads {
                    [&](const Not& v) {
                        return eval(generator, **v.expression);
                    },
                    [&](const NumericComparison&) {
                        return invert(eval(generator, **not_v.expression));
                    },
                    [&](const Or& v) {
                        auto not_lhs = invert(eval(generator, **v.left));
                        auto not_rhs = invert(eval(generator, **v.right));
                        return handle_and(std::move(not_lhs), std::move(not_rhs));
                    },
                    [&](const And& v) {
                        auto not_lhs = invert(eval(generator, **v.left));
                        auto not_rhs = invert(eval(generator, **v.right));
                        return handle_or(std::move(not_lhs), std::move(not_rhs));
                    },
                    [&](const BoolConst&) {
                        return invert(eval(generator, **not_v.expression));
                    },
                    [&](auto&&) {
                        return error();
                    }
                };
                
                const Expression& not_inner = **not_v.expression;
                return std::visit(not_visitor, not_inner.expression);
            },
            [&](auto&&) {
                return error();
            }
            
        };

        return std::visit(visitor, expr.expression);
    }
};