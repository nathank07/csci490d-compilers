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
        std::function<Instruction(std::size_t, int64_t, Cond)> create_instr;
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

                return BooleanChunk{ [compare](auto block_size, auto if_guard_size, auto cond) {
                    auto rv = Architecture::compose(
                        compare,
                        Architecture::jump_rel_when(block_size + if_guard_size, cond)
                    );
                    rv.byte_size += block_size;
                    return rv;
                }, cond };
            },
            [&](const Or& v) {
                auto lhs = eval(generator, **v.left);
                auto rhs = eval(generator, **v.right);

                return BooleanChunk{ [lhs, rhs](auto block_size, auto if_guard_size, auto) {
                    auto rhs_full = rhs.create_instr(block_size, if_guard_size, rhs.cond);
                    return Architecture::compose(
                        lhs.create_instr(rhs_full.byte_size, if_guard_size, lhs.cond),
                        rhs_full
                    );
                }
                , rhs.cond };
            },
            [&](const And& v) {
                auto lhs = eval(generator, **v.left);
                auto rhs = eval(generator, **v.right);

                return BooleanChunk{ [lhs, rhs](auto block_size, auto if_guard_size, auto) {
                    auto rhs_full = rhs.create_instr(block_size - if_guard_size, if_guard_size, Architecture::invert(rhs.cond));
                    return Architecture::compose(
                        lhs.create_instr(rhs_full.byte_size, if_guard_size, Architecture::invert(lhs.cond)),
                        rhs_full
                    );
                }
                , rhs.cond };
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