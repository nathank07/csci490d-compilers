#pragma once
#include "boolInstructions.hpp"
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

    template <typename Generator>
    static BooleanChunk eval(Generator& generator, const Expression& expr) {
        const auto visitor = overloads {
            [&](const NumericComparison& v) {
                auto compare = generator.eval(expr);
                
                auto cond = 
                    StackUtils::assert_cond<typename Generator::Stack::StackUnit, Generator>
                        (generator.stack.pop());

                return BooleanChunk{[compare](auto on_match, auto on_fail, auto cond) {
                    auto jump_on_match = Architecture::jump_rel(on_match);
                    return Architecture::compose(
                        compare,
                        Architecture::jump_rel_when(on_fail + jump_on_match.byte_size, Architecture::invert(cond)),
                        jump_on_match
                    );
                }, cond };
            },
            [&](const Or& v) {
                auto lhs = eval(generator, **v.left);
                auto rhs = eval(generator, **v.right);

                return BooleanChunk{ [lhs, rhs](auto on_match, auto on_fail, auto) {
                    auto _rhs = rhs.create_instr(on_match, on_fail, rhs.cond);
                    auto _lhs = lhs.create_instr(_rhs.byte_size + on_match, 0, lhs.cond);
                    return Architecture::compose(_lhs, _rhs);
                }, rhs.cond };
            },
            [&](const And& v) {
                auto lhs = eval(generator, **v.left);
                auto rhs = eval(generator, **v.right);

                return BooleanChunk{ [lhs, rhs](auto on_match, auto on_fail, auto) {
                    auto _rhs = rhs.create_instr(on_match, on_fail, rhs.cond);
                    auto _lhs = lhs.create_instr(0, on_fail + _rhs.byte_size, lhs.cond);
                    return Architecture::compose(_lhs, _rhs);
                }, rhs.cond };
            },
            [&](auto&&) {
                assert(false);
                return BooleanChunk{ [](auto, auto, auto cond) {
                    return Architecture::compose();
                }, static_cast<typename Architecture::Conditional>(0) };
            }
            
        };

        return std::visit(visitor, expr.expression);
    }
};