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
        std::function<Instruction(Instruction, Cond)> create_instr;
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

                return BooleanChunk{ [compare](auto skip_block, auto cond) {
                    return Architecture::compose(
                        compare,
                        Architecture::skip_block(skip_block, cond)
                    );
                }, cond };
            },
            [&](const Or& v) {
                auto lhs = eval(generator, **v.left);
                auto rhs = eval(generator, **v.right);

                return BooleanChunk{ [lhs, rhs](auto skip_block, auto cond) {
                    auto rhs_full = rhs.create_instr(skip_block, rhs.cond);
                    return lhs.create_instr(rhs_full, lhs.cond);
                }
                , rhs.cond };
            },
            [&](auto&&) {
                return BooleanChunk{ [](auto skip_block, auto cond) {
                    return Architecture::compose();
                }, static_cast<typename Architecture::Conditional>(0) };
            }
            // [&](const Not& v) {
            //     return BooleanChunk {[](auto skip_block, auto cond) {
            //         return Architecture::compose();
            //     }, }
            //     // auto instr = eval(**v.expression);
            //     // auto top = stack.pop();
            //     // if (auto c = StackUtils::maybe_value_u64(top)) {
            //     //     stack.push_const(static_cast<uint64_t>(!static_cast<bool>(*c)));
            //     // }
            //     // return instr;
            // },
            // [&](const And& v) {
            //     // auto instr = eval(**v.left);
            //     // auto top = stack.pop();
            //     // if (auto c = StackUtils::maybe_value_u64(top)) {
            //     //     if (!static_cast<bool>(*c)) {
            //     //         return instr;
            //     //     }
            //     // }
            //     // return instr;
            // },
            
        };

        return std::visit(visitor, expr.expression);
    }
};