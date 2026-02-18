#pragma once

#include "../ast/ast.hpp"
#include "x86operands.hpp"
#include "x86prog.hpp"
#include <functional>
#include <memory>

class x86Generator {

    using Instructions = std::function<void(x86Prog&)>;

    static auto compose(auto f1, auto f2) {
        return [f1, f2](struct x86Prog& p) {
            f1(p);
            f2(p);
        };
    }

    static auto compose(auto f1, auto f2, auto f3) {
        return [f1, f2, f3](struct x86Prog& p) {
            f1(p);
            f2(p);
            f3(p);
        };
    }

    static auto put_stack(long long term) {
        return [term](struct x86Prog& p) {
            p.mov(Register::EAX, Immediate{term});
            p.push(Register::EAX);
        };
    }

    static auto handle_neg() {
        return [](struct x86Prog& p) {
            p.pop(Register::EAX);
            p.neg(Register::EAX);
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
            p.pop(Register::ECX);
            p.pop(Register::EAX);
            p.imul(Register::ECX);
            p.push(Register::EAX);
        };
    }

    static auto handle_div() {
        return [](struct x86Prog& p) {
            p.pop(Register::ECX);
            p.pop(Register::EAX);
            p.cdq();
            p.idiv(Register::ECX);
            p.push(Register::EAX);
        };
    }

    static auto handle_mod() {
        return [](struct x86Prog& p) {
            p.pop(Register::ECX);
            p.pop(Register::EAX);
            p.cdq();
            p.idiv(Register::ECX);
            p.push(Register::EDX);
        };
    }

    static auto handle_exp() {
        return [](struct x86Prog& p) {
            p.xor_(ExtendedRegister::R8, ExtendedRegister::R8);
            p.pop(ExtendedRegister::R10);
            p.pop(ExtendedRegister::R9);
            p.test(ExtendedRegister::R10, ExtendedRegister::R10);
            p.jl(0x1E);
            p.inc(ExtendedRegister::R8);
            p.test(ExtendedRegister::R10, ExtendedRegister::R10);
            p.je(0x16);
            p.test(ExtendedRegister::R10, Immediate{1});
            p.je(0x04);
            p.imul(ExtendedRegister::R8, ExtendedRegister::R9);
            p.imul(ExtendedRegister::R9, ExtendedRegister::R9);
            p.sar1(ExtendedRegister::R10);
            p.jmp(0xE5);
            p.mov(Register::EAX, ExtendedRegister::R8);
            p.push(Register::EAX);
        };
    }

    static Instructions eval(std::unique_ptr<Expression> expr);

public:

    static Instructions generate(std::unique_ptr<Expression> expr) {
        return compose(eval(std::move(expr)), 
            [](x86Prog& p) {
                p.pop(Register::EAX);
                p.ret();
            });
    }
};