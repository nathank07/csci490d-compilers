#pragma once

#include "../../ast/expression.hpp"
#include "../../utils.hpp"
#include "midosProg.hpp"
#include "midosInstructions.hpp"
#include "../stackAllocator.hpp"
#include "midosStackOperator.hpp"
#include "../boolGenerator.hpp"
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <variant>

struct midosGenerator : TypeSize<midosGenerator> {

    static constexpr uint64_t int4_size = 4;
    static constexpr uint64_t stack_size = 4;

    using Instruction = MidOs::Instruction;
    using Register = MidOs::Register;
    using Conditional = MidOs::Conditional;
    using Stack = StackAllocator<midosGenerator>;

    static constexpr Register BasePointer = Register::R3;

    template<typename... Args>
    static Instruction compose(Args&&... args) {
        return MidOs::compose(std::forward<Args>(args)...);
    }

    static constexpr Conditional flip_cond(Conditional c) { return MidOs::flip_cond(c); }
    static constexpr Conditional tok_cond(TokenType t)    { return MidOs::tok_cond(t); }

    static Register primary_scratch()   { return Register::R1; }
    static Register secondary_scratch() { return Register::R2; }

    static Instruction mov_dreg_soffset(Register reg, int32_t offset) {
        return MidOs::load_reg_from_memory(BasePointer, reg, offset);
    }
    static Instruction mov_doffset_sreg(int32_t offset, Register reg) {
        return MidOs::load_memory_from_reg(reg, BasePointer, offset);
    }
    static Instruction mov_dreg_imm(Register reg, uint64_t val) {
        return MidOs::movi(reg, static_cast<int32_t>(val));
    }
    static Instruction mov_doffset_simm(int32_t offset, uint64_t imm) {
        return MidOs::load_immediate_into_memory(BasePointer, static_cast<int32_t>(imm), offset);
    }
    static Instruction mov_dreg_sstatic_ptr(Register reg, uint64_t abs_from_0x0) {
        return MidOs::movi(reg, static_cast<int32_t>(abs_from_0x0));
    }
    static Instruction mov_dreg_sreg(Register lhs, Register rhs) {
        return MidOs::movr(lhs, rhs);
    }

    Stack stack;
    midosStackOperator<midosGenerator> s{stack};

    midosGenerator(Stack& stack_) : stack(stack_) {}

    Instruction eval(const Expression& expr) {

        const auto visitor = overloads {
            [&](const std::monostate&) {
                return MidOs::compose();
            },
            [&](const Term& t) {
                std::visit(overloads {
                    [&](std::string v) {
                        stack.push_identifier(v);
                    },
                    [&](std::u8string) {
                        assert(false && "strings unsupported in MidOs");
                    },
                    [&](long long v) {
                        stack.push_const(static_cast<uint64_t>(v));
                    },
                    [&](double) {
                        assert(false && "doubles unsupported");
                    }
                }, t.v);
                return MidOs::compose();
            },
            [&](const Add& e) {
                auto left  = eval(**e.left);
                auto right = eval(**e.right);
                auto op    = s.perform_binary_op(midosStackOperator<midosGenerator>::midosAdder);
                return MidOs::compose(std::move(left), std::move(right), std::move(op));
            },
            [&](const Sub& e) {
                auto left  = eval(**e.left);
                auto right = eval(**e.right);
                auto op    = s.perform_binary_op(midosStackOperator<midosGenerator>::midosSubber);
                return MidOs::compose(std::move(left), std::move(right), std::move(op));
            },
            [&](const Mult& e) {
                auto left  = eval(**e.left);
                auto right = eval(**e.right);
                auto op    = s.perform_binary_op(midosStackOperator<midosGenerator>::midosMulter);
                return MidOs::compose(std::move(left), std::move(right), std::move(op));
            },
            [&](const Div& e) {
                auto left  = eval(**e.left);
                auto right = eval(**e.right);
                auto op    = s.perform_binary_op(midosStackOperator<midosGenerator>::midosDiver);
                return MidOs::compose(std::move(left), std::move(right), std::move(op));
            },
            [&](const Mod& e) {
                auto left  = eval(**e.left);
                auto right = eval(**e.right);
                auto op    = s.perform_binary_op(midosStackOperator<midosGenerator>::midosModer);
                return MidOs::compose(std::move(left), std::move(right), std::move(op));
            },
            [&](const Exp& e) {
                auto base     = eval(**e.base);
                auto exponent = eval(**e.exponent);
                auto op       = s.perform_binary_op(midosStackOperator<midosGenerator>::midosExper);
                return MidOs::compose(std::move(base), std::move(exponent), std::move(op));
            },
            [&](const Negated& e) {
                auto base = eval(**e.expression);
                auto reg  = primary_scratch();
                auto instr = s.push_reg(reg, std::negate<uint64_t>());
                if (instr) {
                    return MidOs::compose(std::move(base), std::move(*instr), MidOs::negr(reg));
                }
                return base;
            },
            [&](const FunctionCall& v) {
                auto n = v.arg_count();
                auto ident_instr = eval(**v.ident);

                assert(ident_instr.byte_size == 0);

                auto fn = StackUtils::assert_ident(stack.pop());

                MidOs::Instruction args_instr = MidOs::compose();
                if (v.args.is_just()) {
                    auto reversed = std::move(std::get<FunctionCallArgList>((**v.args).expression)).reverse();
                    args_instr = eval(**reversed);
                }

                if (fn == "print") {
                    MidOs::Instruction result = MidOs::compose(std::move(args_instr));
                    for (uint8_t i = 0; i < n; i++) {
                        auto arg = stack.pop();
                        auto reg = primary_scratch();
                        auto load = s.load_reg_from_pop(reg, arg);
                        result = MidOs::compose(
                            std::move(result),
                            std::move(load),
                            MidOs::printr(reg)
                        );
                        stack.unlock_reg(reg);
                    }
                    return result;
                } else if (fn == "read") {
                    assert(n == 1 && "read takes exactly one argument");
                    assert(false && "read unsupported in MidOs");
                    return MidOs::compose();
                } else {
                    assert(false && "unknown function");
                    return MidOs::compose();
                }
            },
            [&](const FunctionCallArgList& v) {
                if (is_logical(*v.value)) {
                    auto reg  = Register::R4;
                    auto load = s.find_free_reg(reg);
                    auto compare  = BoolGenerator<MidOs>::eval(*this, **v.value);
                    auto if_block = MidOs::incr(reg);

                    stack.push(RegisterUnit<midosGenerator>{ reg });
                    stack.push(LogicalAtomicUnit{});

                    return MidOs::compose(
                        load,
                        MidOs::movi(reg, 0),
                        compare.create_instr(0, if_block.byte_size, compare.cond),
                        if_block,
                        v.next.is_just() ? eval(**v.next) : MidOs::compose()
                    );
                }

                auto val = eval(**v.value);
                if (v.next.is_just()) return MidOs::compose(std::move(val), eval(**v.next));
                return val;
            },
            [&](const Declaration&) {
                return MidOs::compose();
            },
            [&](const Assign& v) {
                auto ident  = eval(**v.ident);
                auto value  = eval(**v.value);
                auto assign = s.assign_var_after_eval();
                return MidOs::compose(std::move(ident), std::move(value), std::move(assign));
            },
            [&](const StatementBlock& v) {
                return eval(**v.statements);
            },
            [&](const Statements& v) {
                auto val = eval(**v.value);
                if (v.next.is_just()) return MidOs::compose(std::move(val), eval(**v.next));
                return val;
            },
            [&](const BoolConst& v) {
                stack.push_const(static_cast<uint64_t>(v.is_true));
                stack.push(LogicalAtomicUnit{});
                return MidOs::compose();
            },
            [&](const NumericComparison& v) {
                stack.push(LogicalComparisonUnit<midosGenerator>{ midosGenerator::tok_cond(v.token_type) });
                auto left  = eval(**v.left);
                auto right = eval(**v.right);
                return MidOs::compose(
                    std::move(left), std::move(right),
                    s.perform_comparison_op(midosStackOperator<midosGenerator>::make_comparer())
                );
            },
            [&](const Not&) {
                return MidOs::compose();
            },
            [&](const And&) {
                return MidOs::compose();
            },
            [&](const Or&) {
                return MidOs::compose();
            },
            [&](const If& v) {
                auto if_block = eval(**v.if_statement_block);
                auto compare  = BoolGenerator<MidOs>::eval(*this, **v.logical_expression);
                const auto jmp_size  = MidOs::jump_rel_when(0, compare.cond).byte_size;

                if (!v.else_statement_block) {
                    return MidOs::compose(
                        compare.create_instr(0, if_block.byte_size + jmp_size, compare.cond),
                        if_block
                    );
                }

                auto else_block = eval(**v.else_statement_block);
                if_block = MidOs::compose(
                    if_block,
                    MidOs::jmpi(static_cast<int32_t>(else_block.byte_size))
                );

                return MidOs::compose(
                    compare.create_instr(0, if_block.byte_size + jmp_size, compare.cond),
                    if_block,
                    else_block
                );
            },
            [&](const While& v) {
                auto body    = eval(**v.statement_block);
                auto compare = BoolGenerator<MidOs>::eval(*this, **v.logical_expression);

                // Necessary because all jumps are not the same size.
                const auto jmp_size  = MidOs::jump_rel_when(0, compare.cond).byte_size;

                auto compare_instr   = compare.create_instr(0, body.byte_size + jmp_size, compare.cond);
                auto compare_body    = MidOs::compose(compare_instr, body);

                return MidOs::compose(
                    compare_body,
                    MidOs::jump_rel(static_cast<int32_t>(-(compare_body.byte_size + jmp_size)))
                );
            }
        };

        return std::visit(visitor, expr.expression);
    }

public:

    static void generate(NodeResult expr, const std::unordered_map<std::string, TypedVar>& symbol_table,
                         std::ostream& emitter) {
        midOsProg prog(emitter);
        auto stack = Stack(symbol_table, Register::R4, Register::R5, Register::R6, Register::R7);
        auto generator = midosGenerator(stack);
        auto body = generator.eval(**expr);

        prog.emit(MidOs::compose(
            std::move(body),
            MidOs::_exit()
        ));
    }
};
