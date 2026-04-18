#pragma once

#include "../../ast/expression.hpp"
#include "../../utils.hpp"
#include "x86prog.hpp"
#include "x86Instructions.hpp"
#include "../stackAllocator.hpp"
#include "x86stackOperator.hpp"
#include "../boolGenerator.hpp"
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <variant>
#include <stack>

struct x86Generator : TypeSize<x86Generator> {

    static constexpr uint64_t int4_size = 8;
    static constexpr uint64_t stack_size = 8;

    using Instruction = x86::Instruction;
    using Register = x86::Register;
    using Conditional = x86::Conditional;

    template<typename... Args>
    static Instruction compose(Args&&... args) {
        return x86::compose(std::forward<Args>(args)...);
    }

    // StackOperation interface
    static Register primary_scratch()   { return Register::ECX; }
    static Register secondary_scratch() { return Register::EAX; }

    static Instruction mov_dreg_soffset(Register reg, int32_t offset) {
        return x86::mov_memr_32(Register::EBP, reg, offset);
    }
    static Instruction mov_doffset_sreg(int32_t offset, Register reg) {
        return x86::mov_rmem_64(reg, Register::EBP, offset);
    }
    static Instruction mov_dreg_imm(Register reg, uint64_t val) {
        return x86::mov_64(reg, val);
    }
    static Instruction mov_doffset_simm(int32_t offset, uint64_t imm) {
        return x86::mov_memi_32(Register::EBP, imm, offset);
    }
    static Instruction mov_dreg_sstatic_ptr(Register reg, uint64_t abs_from_0x0) {
        return x86::mov_abs(reg, abs_from_0x0);
    }
    static Instruction mov_dreg_sreg(Register lhs, Register rhs) {
        return x86::mov(lhs, rhs);
    }

    using Stack = StackAllocator<x86Generator>;

    Stack stack;
    const std::map<std::u8string, std::size_t> str_locs;
    x86StackOperator<x86Generator> s{stack};

    // jump_rel varies on how much string data there is, so we store it
    // here so that the u8string variant can get the correct abs addr
    x86::Instruction skip_strs = x86::compose();

    x86Generator(Stack& stack_, const std::map<std::u8string, std::size_t>& str_locs_)
        : stack(stack_), str_locs(str_locs_) {
        if (!str_locs.empty()) {
            auto it = std::max_element(str_locs.begin(), str_locs.end(),
                [](auto& a, auto& b) { return a.second < b.second; });
            skip_strs = x86::jmp(it->second + it->first.size());
        }
    }

    x86::Instruction eval(const Expression& expr) {

        const auto visitor = overloads {
            [&](const std::monostate&) {
                return x86::compose();
            },
            [&](const Term& t) {
                std::visit(overloads {
                    [&](std::string v) {
                        stack.push_identifier(v);
                    },
                    [&](std::u8string v) {
                        auto jmp_size = skip_strs.byte_size;
                        std::u8string key = v += u8'\0';
                        stack.push_static_ptr(str_locs.at(key) + jmp_size);
                    },
                    [&](long long v) {
                        stack.push_const(static_cast<uint64_t>(v));
                    },
                    [&](double) {
                        assert(false && "doubles unsupported");
                    }
                }, t.v);
                return x86::compose();
            },
            // Note: you can't do something like this:
            // compose(eval(left), eval(right), perform_binary)
            // because the variadic template is not guarenteed to be
            // in order, causing side effects to fire in random order.
            [&](const Add& e) {
                auto left  = eval(**e.left);
                auto right = eval(**e.right);
                auto op    = s.perform_binary_op(x86StackOperator<x86Generator>::x86adder);
                return x86::compose(std::move(left), std::move(right), std::move(op));
            },
            [&](const Sub& e) {
                auto left  = eval(**e.left);
                auto right = eval(**e.right);
                auto op    = s.perform_binary_op(x86StackOperator<x86Generator>::x86subber);
                return x86::compose(std::move(left), std::move(right), std::move(op));
            },
            [&](const Mult& e) {
                auto left  = eval(**e.left);
                auto right = eval(**e.right);
                auto op    = s.perform_binary_op(x86StackOperator<x86Generator>::x86multer);
                return x86::compose(std::move(left), std::move(right), std::move(op));
            },
            [&](const Div& e) {
                auto left  = eval(**e.left);
                auto right = eval(**e.right);
                auto op    = s.handle_div_family(x86StackOperator<x86Generator>::DivType::DIV);
                return x86::compose(std::move(left), std::move(right), std::move(op));
            },
            [&](const Mod& e) {
                auto left  = eval(**e.left);
                auto right = eval(**e.right);
                auto op    = s.handle_div_family(x86StackOperator<x86Generator>::DivType::MOD);
                return x86::compose(std::move(left), std::move(right), std::move(op));
            },
            [&](const Exp& e) {
                auto base     = eval(**e.base);
                auto exponent = eval(**e.exponent);
                auto op       = s.perform_binary_op(x86StackOperator<x86Generator>::x86exper);
                return x86::compose(std::move(base), std::move(exponent), std::move(op));
            },
            [&](const Negated& e) {
                auto base = eval(**e.expression);
                auto reg = primary_scratch();
                auto instr = s.push_reg(reg, std::negate<uint64_t>());
                if (instr) {
                    return x86::compose(
                        std::move(base),
                        std::move(*instr),
                        x86::neg(reg)
                    );
                }
                return base;
            },
            [&](const FunctionCall& v) {
                auto n = v.arg_count();
                auto ident_instr = eval(**v.ident);
                auto fn = StackUtils::assert_ident(stack.pop());

                x86::Instruction args_instr = x86::compose();
                if (v.args.is_just()) args_instr = eval(**v.args);

                // ECX can get overwritten by the SysV ABI so we evict it to
                // another register to save it; we have to do this because the
                // args get popped to the C++ stack; and our abstracted stack
                // cannot lock values in the C++ stack (it thinks it already
                // got popped, and that it doesn't need to be protected.)
                x86::Instruction free_ecx = x86::compose();
                if (fn == "print" || fn == "read") {
                    free_ecx = s.evict_space_for(Register::ECX);
                }

                std::stack<Stack::StackUnit> args;
                for (uint8_t i = 0; i < n; i++) args.push(stack.pop());

                if (fn == "print") {
                    auto pre_print_size = stack.size();
                    x86::Instruction result = x86::compose(
                        std::move(ident_instr),
                        std::move(args_instr),
                        std::move(free_ecx),
                        x86::align_sp_start(pre_print_size)
                    );
                    while (!args.empty()) {
                        auto arg = std::move(args.top()); args.pop();
                        auto reg = Register::EAX;
                        auto is_str = StackUtils::maybe_static_ptr(arg);
                        auto load = s.load_reg_from_pop(reg, arg);
                        result = x86::compose(
                            std::move(result),
                            std::move(load),
                            is_str ?
                                x86::print_char_addr(reg) :
                                x86::print_num_literal(reg)
                        );
                        stack.unlock_reg(reg);
                    }
                    stack.unlock_reg(Register::ECX);
                    return x86::compose(std::move(result), x86::align_sp_end(pre_print_size));

                } else if (fn == "read") {
                    assert(n == 1 && "read takes exactly one argument");
                    auto arg = std::move(args.top()); args.pop();
                    auto id = StackUtils::assert_ident(arg);
                    stack.unlock_reg(Register::ECX);
                    return x86::compose(
                        std::move(ident_instr),
                        std::move(args_instr),
                        std::move(free_ecx),
                        x86::read_int4(x86::Register::EAX, stack.size()),
                        x86::mov_rmem_64(x86::Register::EAX, x86::Register::EBP, stack.get(id))
                    );
                } else {
                    assert(false && "unknown function");
                    return x86::compose();
                }
            },
            [&](const FunctionCallArgList& v) {
                auto val = eval(**v.value);
                if (v.next.is_just()) return x86::compose(std::move(val), eval(**v.next));
                return val;
            },
            [&](const Declaration&) {
                return x86::compose();
            },
            [&](const Assign& v) {
                auto ident  = eval(**v.ident);
                auto value  = eval(**v.value);
                auto assign = s.assign_var_after_eval();
                return x86::compose(std::move(ident), std::move(value), std::move(assign));
            },
            [&](const StatementBlock& v) {
                return eval(**v.statements);
            },
            [&](const Statements& v) {
                auto val = eval(**v.value);
                if (v.next.is_just()) return x86::compose(std::move(val), eval(**v.next));
                return val;
            },
            [&](const BoolConst& v) {
                stack.push_const(static_cast<uint64_t>(v.is_true));
                return x86::compose();
            },
            [&](const NumericComparison& v) {
                auto left  = eval(**v.left);
                auto right = eval(**v.right);
                auto rhs = stack.pop();
                auto lhs = stack.pop();

                auto lhs_reg = primary_scratch();
                auto rhs_reg = secondary_scratch();

                // We normalize the regs into a register, but then immediately
                // compare them; this makes locking them redundant, so we unlock
                // them so other arithmetic can use it. Locking is more of a useful
                // usecase if we're pushing a result, but it's already stored
                // in the sign flags.
                auto load_l = s.load_reg_from_pop(lhs_reg, lhs);
                auto load_r = s.load_reg_from_pop(rhs_reg, rhs);

                stack.unlock_reg(lhs_reg);
                stack.unlock_reg(rhs_reg);

                stack.push(LogicalComparisonUnit<x86Generator> { 
                    x86::tok_cond(v.token_type) 
                });

                return x86::compose(
                    std::move(left), std::move(right),
                    std::move(load_l), std::move(load_r),
                    x86::compare(lhs_reg, rhs_reg)
                );
            },
            [&](const Not& v) {
                // <Handled by boolGenerator>
                return x86::compose();
            },
            [&](const And& v) {
                // <Handled by boolGenerator>
                return x86::compose();
            },
            [&](const Or& v) {
                // <Handled by boolGenerator>
                return x86::compose();
            },
            [&](const If& v) {
                
                auto if_block = eval(**v.if_statement_block);
                auto compare = BoolGenerator<x86>::eval(*this, **v.logical_expression);


                if (!v.else_statement_block) {
                    return x86::compose(
                        compare.create_instr(0, if_block.byte_size, compare.cond),
                        if_block
                    );
                }

                auto else_block = eval(**v.else_statement_block);

                if_block = x86::compose(
                    if_block,
                    x86::jump_rel(else_block.byte_size)
                );

                return x86::compose(
                    compare.create_instr(0, if_block.byte_size, compare.cond),
                    if_block,
                    else_block
                );
            },
            [&](const While& v) {
                
                auto body = eval(**v.statement_block);
                auto compare = BoolGenerator<x86>::eval(*this, **v.logical_expression);


                const auto jmp_size = x86::jmp32(0).byte_size;
                auto compare_instr = compare.create_instr(0, body.byte_size + jmp_size, compare.cond);
                
                auto compare_body = x86::compose(
                    compare_instr,
                    body
                );

                return x86::compose(
                    compare_body,
                    x86::jmp32(-(compare_body.byte_size + jmp_size))
                );
            }
        };

        return std::visit(visitor, expr.expression);
    }

public:

    static x86Prog generate(NodeResult expr, const std::unordered_map<std::string, TypedVar>& symbol_table,
                                             const std::map<std::u8string, std::size_t> str_locs) {
        x86Prog prog;
        auto stack = Stack(symbol_table, Register::R12, Register::R13, Register::R14, Register::R15);
        auto generator = x86Generator(stack, str_locs);
        auto body = generator.eval(**expr);
        auto stack_size = static_cast<int32_t>(generator.stack.size());
        x86::Instruction string_pool;

        // sort by value, because map is sorted alphabetically, and the strings
        // probably aren't in alphabetical order - so you'll be pointing to
        // garbage data
        std::vector<std::pair<std::u8string, std::size_t>>
            sorted_strs(str_locs.begin(), str_locs.end());
        std::sort(sorted_strs.begin(), sorted_strs.end(), [](auto& a, auto& b)
            { return a.second < b.second; });

        for (const auto& str : sorted_strs) {
            string_pool = x86::compose(
                std::move(string_pool),
                x86::write_raw_string(str.first)
            );
        }

        prog.prog_fn = x86::compose(
            std::move(generator.skip_strs),
            std::move(string_pool),
            x86::push(x86::Register::EBP),
            x86::sub(x86::Register::ESP, stack_size),
            x86::mov(x86::Register::EBP, x86::Register::ESP),
            std::move(body),
            x86::add(x86::Register::ESP, stack_size),
            x86::pop(x86::Register::EBP),
            x86::ret()
        );
        return prog;
    }
};
