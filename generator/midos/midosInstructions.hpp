#pragma once
#include <cassert>
#include <cstdint>
#include <string>
#include <ostream>
#include <utility>
#include <functional>
#include "../baseInstruction.hpp"

struct MidOs : InstructionControl<MidOs> {

    enum class Register {
        R1 = 1, R2, R3, R4, R5,
        R6, R7, R8, R9, R10
    };

    enum class Conditional {
        GT, LT, EQ
    };

    static constexpr size_t INSTRUCTION_SIZE = 9;

    using Instruction = BaseInstruction;

    static std::string get_register(Register r) {
        return "r" + std::to_string(static_cast<int>(r));
    }

    // ** CRTP Instructions **
    static Instruction compose() { return {}; }

    template<typename... Args>
    static Instruction compose(Instruction first, Args&&... rest) {
        auto remaining = compose(std::forward<Args>(rest)...);
        first.emitted_content += remaining.emitted_content;
        first.byte_size += remaining.byte_size;
        return first;
    }

    static Instruction compare(Register r1, Register r2) {
        return cmpr(r1, r2);
    }

    static Instruction compare(Register r, int32_t v) {
        return cmpi(r, v);
    }

    static Instruction jump_rel(int32_t addr) {
        if (addr > 0) addr += static_cast<int32_t>(INSTRUCTION_SIZE);
        return jmpi(addr);
    }

    static Instruction jump_rel_when(int32_t addr, Conditional cond) {
        if (addr > 0) addr += static_cast<int32_t>(INSTRUCTION_SIZE);
        return jmp_with_flag(addr, cond);
    }

    // ** End CRTP Instructions **

    static Instruction comment(std::string comment) {
        return {{";" + comment + "\n", 0}};
    }

    // Base instructions

    static Instruction addr(Register r1, Register r2) {
        return {{"addr " + get_register(r1) + ", " + get_register(r2) + "\n", INSTRUCTION_SIZE}};
    }
    static Instruction addi(Register r, int32_t v) {
        return {{"addi " + get_register(r) + ", #" + std::to_string(v) + "\n", INSTRUCTION_SIZE}};
    }
    static Instruction incr(Register r) {
        return {{"incr " + get_register(r) + "\n", INSTRUCTION_SIZE}};
    }
    static Instruction movi(Register r, int32_t v) {
        return {{"movi " + get_register(r) + ", #" + std::to_string(v) + "\n", INSTRUCTION_SIZE}};
    }
    static Instruction movr(Register r1, Register r2) {
        return {{"movr " + get_register(r1) + ", " + get_register(r2) + "\n", INSTRUCTION_SIZE}};
    }
    static Instruction pushr(Register r) {
        return {{"pushr " + get_register(r) + "\n", INSTRUCTION_SIZE}};
    }
    static Instruction pushi(int32_t v) {
        return {{"pushi #" + std::to_string(v) + "\n", INSTRUCTION_SIZE}};
    }
    static Instruction popr(Register r) {
        return {{"popr " + get_register(r) + "\n", INSTRUCTION_SIZE}};
    }
    static Instruction unsafe_cmpi(Register r, int32_t v) {
        return {{"cmpi " + get_register(r) + ", #" + std::to_string(v) + "\n", INSTRUCTION_SIZE}};
    }
    static Instruction unsafe_cmpr(Register r1, Register r2) {
        return {{"cmpr " + get_register(r1) + ", " + get_register(r2) + "\n", INSTRUCTION_SIZE}};
    }
    static Instruction jmpi(int32_t offset) {
        return {{"jmpi #" + std::to_string(offset) + "\n", INSTRUCTION_SIZE}};
    }
    static Instruction jmpa(uint32_t a) {
        return {{"jmpa #" + std::to_string(a) + "\n", INSTRUCTION_SIZE}};
    }
    static Instruction jei(int32_t offset) {
        return {{"jei #" + std::to_string(offset) + "\n", INSTRUCTION_SIZE}};
    }
    static Instruction jea(uint32_t a) {
        return {{"jea #" + std::to_string(a) + "\n", INSTRUCTION_SIZE}};
    }
    static Instruction jlti(int32_t offset) {
        return {{"jlti #" + std::to_string(offset) + "\n", INSTRUCTION_SIZE}};
    }
    static Instruction jlta(uint32_t a) {
        return {{"jlta #" + std::to_string(a) + "\n", INSTRUCTION_SIZE}};
    }
    static Instruction jgti(int32_t offset) {
        return {{"jgti #" + std::to_string(offset) + "\n", INSTRUCTION_SIZE}};
    }
    static Instruction jgta(uint32_t a) {
        return {{"jgta #" + std::to_string(a) + "\n", INSTRUCTION_SIZE}};
    }
    static Instruction printr(Register r) {
        return {{"printr " + get_register(r) + "\n", INSTRUCTION_SIZE}};
    }
    static Instruction printm(Register r) {
        return {{"printm " + get_register(r) + "\n", INSTRUCTION_SIZE}};
    }
    static Instruction printcr(Register r) {
        return {{"printcr " + get_register(r) + "\n", INSTRUCTION_SIZE}};
    }
    static Instruction printcm(Register r) {
        return {{"printcm " + get_register(r) + "\n", INSTRUCTION_SIZE}};
    }
    static Instruction call(Register r) {
        return {{"call " + get_register(r) + "\n", INSTRUCTION_SIZE}};
    }
    static Instruction ret() {
        return {{"ret\n", INSTRUCTION_SIZE}};
    }
    static Instruction _exit() {
        return {{"exit\n", INSTRUCTION_SIZE}};
    }

    // End base instructions

    static Instruction jmp_with_flag(int32_t rel_addr, Conditional on_cond) {
        switch (on_cond) {
            case Conditional::GT: return jgti(rel_addr);
            case Conditional::LT: return jlti(rel_addr);
            case Conditional::EQ: return jei(rel_addr);
        }
        __builtin_unreachable();
    }

    // **Warning: Modifies flags. Alias for unsafe_cmpi(r1, v)**
    static Instruction subi(Register r, uint32_t v) {
        return unsafe_cmpi(r, v);
    }

    // **Warning: Modifies flags. Alias for unsafe_cmpr(r1, r2)**
    static Instruction subr(Register r1, Register r2) {
        return unsafe_cmpr(r1, r2);
    }

    // Helper function to compare registers without modifying r1
    static Instruction cmpr(Register r1, Register r2) {
        return compose(
            pushr(r1),
            unsafe_cmpr(r1, r2),
            popr(r1)
        );
    }

    static Instruction cmpi(Register r1, int32_t i) {
        return compose(
            pushr(r1),
            unsafe_cmpi(r1, i),
            popr(r1)
        );
    }

    static Instruction negr(Register r) {
        // It's important to note that you can't use this pattern to make the
        // conditional unsafe constructs safe. This is because negr isn't executing
        // any code inside of it, so restoring the util reg doesn't silently
        // overwrite code inside of it. This is safe explicitly because the
        // only thing being done with the scratch reg is using it as a temporary
        // immediate value.
        Register scratch = static_cast<int>(r) == 10 ?
            Register::R9 : Register::R10;

        return compose(
            pushr(scratch),
            subr(scratch, scratch),
            subr(scratch, r),
            movr(r, scratch),
            popr(scratch)
        );
    }

    // **Warning: Can clobber r1 if not used with 0. Uses unsafe_cmpr(r1, r2)**
    static Instruction unsafe_skip_if(Register r1, Register r2, Conditional cond, Instruction do_this) {
        return skip_if(unsafe_cmpr(r1, r2), cond, do_this);
    }

    static Instruction unsafe_skip_if(Register r, int32_t v, Conditional cond, Instruction do_this) {
        return skip_if(unsafe_cmpi(r, v), cond, do_this);
    }

    // Unsafe because it uses unsafe_cmpi(r, v) for comparison, which subs v from r.
    // This means that if v != 0, you probably don't want to use this.
    static Instruction unsafe_do_while(Register r, int32_t v, Conditional cond, Instruction do_this) {
        return do_while(unsafe_cmpi(r, v), cond, do_this);
    }

    // Uses unsafe_cmpr(r1, r2) - which subtracts r2 from r1
    static Instruction unsafe_while(Register r1, Register r2, Conditional cond, Instruction do_this) {
        return _while(unsafe_cmpr(r1, r2), cond, do_this);
    }

    // Uses unsafe_cmpr(r1, v) - which subtracts v from r
    static Instruction unsafe_while(Register r, int32_t v, Conditional cond, Instruction do_this) {
        return _while(unsafe_cmpi(r, v), cond, do_this);
    }

    // **Uses R2, R9 and R10.**
    static Instruction unsafe_multr(Register r1, Register r2) {
        Register seed = r1;
        Register counter = r2;
        Register accumulator = Register::R9;
        Register rhs_is_neg = Register::R10;

        assert(r1 != accumulator && r1 != rhs_is_neg);
        assert(r2 != accumulator && r2 != rhs_is_neg);

        return compose(
            movi(accumulator,  0),
            movi(rhs_is_neg, 0),
            // unsafe skip_if OK here because it's comparing against 0.
            // If R9 is checked, then we negate it again afterwards.
            // We do this because the counter subtracts when accumulating
            // so it prevents an infinite loop
            unsafe_skip_if(counter, accumulator, Conditional::GT, compose(
                negr(counter),
                incr(rhs_is_neg)
            )),
            InstructionControl::_while(counter, 0, Conditional::GT, compose(
                addr(accumulator, seed),
                addi(counter, -1)
            )),
            unsafe_skip_if(rhs_is_neg, 0, Conditional::EQ, compose(
                negr(accumulator)
            )),
            movr(r1, accumulator)
        );
    }

    // **Uses R8, R9, and R10.**
    // Puts the quotient in r1, and the remainder in r2. Has C-style mod behavior.
    static Instruction unsafe_divr(Register r1, Register r2) {
        Register accumulator = r1;
        Register seed = r2;
        Register lhs_is_neg = Register::R8;
        Register rhs_is_neg = Register::R9;
        Register quotient = Register::R10;

        assert(r1 != lhs_is_neg && r1 != rhs_is_neg && r1 != quotient);
        assert(r2 != lhs_is_neg && r2 != rhs_is_neg && r2 != quotient);

        return compose(
            movi(lhs_is_neg, 0),
            movi(rhs_is_neg, 0),
            movi(quotient, 0),
            // Normalize LHS
            unsafe_skip_if(accumulator, 0, Conditional::GT, compose(
                negr(accumulator),
                incr(lhs_is_neg)
            )),
            // Normalize RHS
            unsafe_skip_if(seed, 0, Conditional::GT, compose(
                negr(seed),
                incr(rhs_is_neg)
            )),
            // Subtract until finding value. Do while instead of while is actually
            // preferable here, because when it overshoots, we get the mod value.
            unsafe_do_while(accumulator, 0, Conditional::GT, compose(
                subr(accumulator, seed),
                incr(quotient)
            )),
            // Check if we overshot. if so, go backwards. Also by adding the seed
            // to the accumulator, we get the mod, which is a nice touch.
            unsafe_skip_if(accumulator, 0, Conditional::EQ, compose(
                subi(quotient, 1),
                addr(accumulator, seed)
            )),
            // Negate values as needed, since they were normalized earlier.
            InstructionControl::skip_if(lhs_is_neg, rhs_is_neg, Conditional::EQ, negr(quotient)),
            unsafe_skip_if(lhs_is_neg, 0, Conditional::EQ, negr(accumulator)),
            movr(r2, accumulator),
            movr(r1, quotient)
        );
    }

    // ** Uses R2, R7, R8, R9, and R10 **
    static Instruction unsafe_expr(Register r1, Register r2) {
        Register accumulator = r1;
        Register exp_counter = r2;
        Register base = Register::R7;

        assert(r1 != base && r2 != base);

        return compose(
            movr(base, accumulator),
            movi(accumulator, 1),
            InstructionControl::_while(exp_counter, 0, Conditional::GT, compose(
                // unsafe_multr clobbers R2, so pushing and popping is necessary here.
                pushr(base),
                unsafe_multr(accumulator, base),
                popr(base),
                subi(exp_counter, 1)
            ))
        );
    }

    static Instruction expr(Register r1, Register r2) {
        return compose(
            pushr(r2),
            pushr(Register::R7),
            pushr(Register::R8),
            pushr(Register::R9),
            pushr(Register::R10),
            unsafe_expr(r1, r2),
            popr(Register::R10),
            popr(Register::R9),
            popr(Register::R8),
            popr(Register::R7),
            popr(r2)
        );
    }

    static Instruction multr(Register r1, Register r2) {
        return compose(
            pushr(r2),
            pushr(Register::R8),
            pushr(Register::R9),
            pushr(Register::R10),
            unsafe_multr(r1, r2),
            popr(Register::R10),
            popr(Register::R9),
            popr(Register::R8),
            popr(r2)
        );
    }

    static Instruction divr(Register r1, Register r2) {
        return compose(
            pushr(Register::R8),
            pushr(Register::R9),
            pushr(Register::R10),
            unsafe_divr(r1, r2),
            popr(Register::R10),
            popr(Register::R9),
            popr(Register::R8)
        );
    }
};
