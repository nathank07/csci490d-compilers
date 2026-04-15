#pragma once

#include "../../ast/ast.hpp"
#include "../../utils.hpp"
#include "x86prog.hpp"
#include "x86Instructions.hpp"
#include "../stackAllocator.hpp"
#include "x86stackOperator.hpp"
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

    template<typename... Args>
    static Instruction compose(Args&&... args) {
        return x86::compose(std::forward<Args>(args)...);
    }

    // StackOperation interface
    static Register primary_scratch()   { return Register::ECX; }
    static Register secondary_scratch() { return Register::EAX; }

    static Instruction mov_dreg_soffset(Register reg, int32_t offset) {
        return x86::mov_memr_32(reg, Register::EBP, offset);
    }
    static Instruction mov_doffset_sreg(int32_t offset, Register reg) {
        return x86::mov_rmem_64(Register::EBP, reg, offset);
    }
    static Instruction mov_dreg_imm(Register reg, uint64_t val) {
        return x86::mov_64(reg, val);
    }
    static Instruction mov_dreg_sstatic_ptr(Register reg, uint64_t abs_from_0x0) {
        return x86::mov_abs(reg, abs_from_0x0);
    }
    static Instruction mov_dreg_sreg(Register lhs, Register rhs) {
        return x86::mov(lhs, rhs);
    }

private:

    using Stack = StackAllocator<x86Generator, x86::Register>;

    Stack stack;
    const std::map<std::u8string, std::size_t> str_locs;
    x86StackOperator<x86Generator> s{stack};
    x86Prog& p;
    
    // jump_rel varies on how much string data there is, so we store it
    // here so that the u8string variant can get the correct abs addr
    x86::Instruction skip_strs = x86::compose();

    x86Generator(x86Prog& p_, Stack& stack_, const std::map<std::u8string, std::size_t>& str_locs_)
        : stack(stack_), str_locs(str_locs_), p(p_) {
        if (!str_locs.empty()) {
            auto it = std::max_element(str_locs.begin(), str_locs.end(),
                [](auto& a, auto& b) { return a.second < b.second; });
            skip_strs = x86::jump_rel(it->second + it->first.size());
        }
    }

    void eval(std::unique_ptr<Expression> expr) {

        const auto visitor = overloads {
            [&](std::monostate&) {},
            [&](Term& t) {
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
            },
            [&](const BoolConst& v) {
                stack.push_const(static_cast<uint64_t>(v.is_true));
            },
            [&](Add& e) {
                eval(std::move(*e.left));
                eval(std::move(*e.right));
                p.append(s.perform_binary_op(x86StackOperator<x86Generator>::x86adder));
            },
            [&](Sub& e) {
                eval(std::move(*e.left));
                eval(std::move(*e.right));
                p.append(s.perform_binary_op(x86StackOperator<x86Generator>::x86subber));
            },
            [&](Mult& e) {
                eval(std::move(*e.left));
                eval(std::move(*e.right));
                p.append(s.perform_binary_op(x86StackOperator<x86Generator>::x86multer));
            },
            [&](Div& e) {
                eval(std::move(*e.left));
                eval(std::move(*e.right));
                p.append(s.handle_div_family(
                    x86StackOperator<x86Generator>::DivType::DIV
                ));
            },
            [&](Mod& e) {
                eval(std::move(*e.left));
                eval(std::move(*e.right));
                p.append(s.handle_div_family(
                    x86StackOperator<x86Generator>::DivType::MOD
                ));
            },
            [&](Exp& e) {
                eval(std::move(*e.base));
                eval(std::move(*e.exponent));
                p.append(s.perform_binary_op(x86StackOperator<x86Generator>::x86exper));
            },
            [&](Negated& e) {
                eval(std::move(*e.expression));
                auto reg = primary_scratch();
                auto instr = s.push_reg(reg, std::negate<uint64_t>());
                if (instr) {
                    p.append(x86::compose(
                        std::move(*instr),
                        x86::neg(reg)
                    ));
                }
            },
            [&](FunctionCall& v) {
                auto n = v.arg_count();
                eval(std::move(*v.ident));
                auto fn = StackUtils::assert_ident(stack.pop());

                if (v.args.is_just()) eval(std::move(*v.args));

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
                    p.append(x86::compose(
                        std::move(free_ecx),
                        x86::align_sp_start(pre_print_size)
                    ));
                    while (!args.empty()) {
                        auto arg = std::move(args.top()); args.pop();
                        auto reg = Register::EAX;
                        auto is_str = StackUtils::maybe_static_ptr(arg);
                        auto load = s.load_reg_from_pop(reg, arg);
                        p.append(x86::compose(
                            std::move(load),
                            is_str ?
                                x86::print_char_addr(reg) :
                                x86::print_num_literal(reg)
                        ));
                        stack.unlock_reg(reg);
                    }
                    stack.unlock_reg(Register::ECX);
                    p.append(x86::align_sp_end(pre_print_size));

                } else if (fn == "read") {
                    assert(n == 1 && "read takes exactly one argument");
                    auto arg = std::move(args.top()); args.pop();
                    auto id = StackUtils::assert_ident(arg);
                    p.append(x86::compose(
                        std::move(free_ecx),
                        x86::read_int4(x86::Register::EAX, stack.size()),
                        x86::mov_rmem_64(x86::Register::EBP, x86::Register::EAX, stack.get(id))
                    ));
                    stack.unlock_reg(Register::ECX);
                } else {
                    assert(false && "unknown function");
                }
            },
            [&](FunctionCallArgList& v) {
                eval(std::move(*v.value));
                if (v.next.is_just()) eval(std::move(*v.next));
            },
            [&](Declaration&) {},
            [&](Assign& v) {
                eval(std::move(*v.ident));
                auto ident_unit = stack.pop();
                auto offset = static_cast<int32_t>(stack.get(StackUtils::assert_ident(ident_unit)));
                eval(std::move(*v.value));
                auto top = stack.pop();
                std::visit(overloads {
                    [&](ValueUnit& u)    { 
                        p.append(x86::mov_mem_32(x86::Register::EBP, static_cast<int32_t>(u.literal), offset)); 
                    },
                    [&](VirtualRegisterUnit& u) {
                        p.append(x86::mov_memr_32(x86::Register::EAX, x86::Register::EBP, stack.get_vreg(u.sp_idx)));
                        p.append(x86::mov_rmem_64(x86::Register::EBP, x86::Register::EAX, offset));
                    },
                    [&](RegisterUnit<x86::Register>& u) {
                        p.append(x86::mov_rmem_64(x86::Register::EBP, u.in_register, offset));
                    },
                    [&](StaticPointerUnit&) { assert(false && "Variable strings not supported"); },
                    [&](IdentifierUnit& id) {
                        p.append(x86::mov_memr_32(x86::Register::EAX, x86::Register::EBP, stack.get(id.ident)));
                        p.append(x86::mov_rmem_64(x86::Register::EBP, x86::Register::EAX, offset));
                    }
                }, top);
            },
            [&](StatementBlock& v) {
                eval(std::move(*v.statements));
            },
            [&](Statements& v) {
                eval(std::move(*v.value));
                if (v.next.is_just()) eval(std::move(*v.next));
            },
            [&](NumericComparison& v) {
                eval(std::move(*v.left));
                eval(std::move(*v.right));
            },
            [&](Not& v) {
                
            },
            [&](And& v) {
               
            },
            [&](Or& v) {
                
            },
            [&](If& v) {
                
            },
            [&](While& v) {
               
            }
        };

        std::visit(visitor, expr->expression);
    }

public:

    static x86Prog generate(NodeResult expr, const std::unordered_map<std::string, TypedVar>& symbol_table, 
                                             const std::map<std::u8string, std::size_t> str_locs) {
        x86Prog prog;
        auto stack = Stack(symbol_table, Register::R12, Register::R13, Register::R14, Register::R15);
        auto generator = x86Generator(prog, stack, str_locs);
        generator.eval(std::move(*expr));
        auto stack_size = static_cast<int32_t>(stack.size());
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
            prog.prog_fn,
            x86::add(x86::Register::ESP, stack_size),
            x86::pop(x86::Register::EBP),
            x86::ret()
        );
        return prog;
    }
};
