#pragma once
#include <cassert>
#include <cstdint>
#include <string>
#include <ostream>
#include <utility>
#include <functional>

enum class Register {
    R1 = 1, R2, R3, R4, R5,
    R6, R7, R8, R9, R10
};

static constexpr size_t INSTRUCTION_SIZE = 9;

namespace MidOs {
    static std::string get_register(Register r) {
        return "r" + std::to_string(static_cast<int>(r));
    }
}

enum class Conditional {
    GT, LT, EQ,
};

struct Instruction {
    std::string content;
    size_t instruction_count = 0;

    size_t size() const { return instruction_count * INSTRUCTION_SIZE; }

    void operator()(std::ostream& os) const { os << content; }

    friend std::ostream& operator<<(std::ostream& os, const Instruction& b) {
        os << b.content;
        return os;
    }
};

inline Instruction compose() { return {}; }

template<typename... Args>
inline Instruction compose(Instruction first, Args&&... rest) {
    auto remaining = compose(std::forward<Args>(rest)...);
    first.content += remaining.content;
    first.instruction_count += remaining.instruction_count;
    return first;
}

inline Instruction comment(std::string comment) {
    return {";" + comment + "\n", 0};
}

// Base instructions — each is a single 9-byte instruction
inline Instruction addr(Register r1, Register r2) {
    return {"addr " + MidOs::get_register(r1) + ", " + MidOs::get_register(r2) + "\n", 1};
}
inline Instruction addi(Register r, int32_t v) {
    return {"addi " + MidOs::get_register(r) + ", #" + std::to_string(v) + "\n", 1};
}
inline Instruction incr(Register r) {
    return {"incr " + MidOs::get_register(r) + "\n", 1};
}
inline Instruction movi(Register r, int32_t v) {
    return {"movi " + MidOs::get_register(r) + ", #" + std::to_string(v) + "\n", 1};
}
inline Instruction movr(Register r1, Register r2) {
    return {"movr " + MidOs::get_register(r1) + ", " + MidOs::get_register(r2) + "\n", 1};
}
inline Instruction pushr(Register r) {
    return {"pushr " + MidOs::get_register(r) + "\n", 1};
}
inline Instruction pushi(int32_t v) {
    return {"pushi #" + std::to_string(v) + "\n", 1};
}
inline Instruction popr(Register r) {
    return {"popr " + MidOs::get_register(r) + "\n", 1};
}
inline Instruction unsafe_cmpi(Register r, int32_t v) {
    return {"cmpi " + MidOs::get_register(r) + ", #" + std::to_string(v) + "\n", 1};
}
inline Instruction unsafe_cmpr(Register r1, Register r2) {
    return {"cmpr " + MidOs::get_register(r1) + ", " + MidOs::get_register(r2) + "\n", 1};
}
inline Instruction jmpi(int32_t offset) {
    return {"jmpi #" + std::to_string(offset) + "\n", 1};
}
inline Instruction jmpa(uint32_t a) {
    return {"jmpa #" + std::to_string(a) + "\n", 1};
}
inline Instruction jei(int32_t offset) {
    return {"jei #" + std::to_string(offset) + "\n", 1};
}
inline Instruction jea(uint32_t a) {
    return {"jea #" + std::to_string(a) + "\n", 1};
}
inline Instruction jlti(int32_t offset) {
    return {"jlti #" + std::to_string(offset) + "\n", 1};
}
inline Instruction jlta(uint32_t a) {
    return {"jlta #" + std::to_string(a) + "\n", 1};
}
inline Instruction jgti(int32_t offset) {
    return {"jgti #" + std::to_string(offset) + "\n", 1};
}
inline Instruction jgta(uint32_t a) {
    return {"jgta #" + std::to_string(a) + "\n", 1};
}
inline Instruction printr(Register r) {
    return {"printr " + MidOs::get_register(r) + "\n", 1};
}
inline Instruction printm(Register r) {
    return {"printm " + MidOs::get_register(r) + "\n", 1};
}
inline Instruction printcr(Register r) {
    return {"printcr " + MidOs::get_register(r) + "\n", 1};
}
inline Instruction printcm(Register r) {
    return {"printcm " + MidOs::get_register(r) + "\n", 1};
}
inline Instruction call(Register r) {
    return {"call " + MidOs::get_register(r) + "\n", 1};
}
inline Instruction ret() {
    return {"ret\n", 1};
}
inline Instruction _exit() {
    return {"exit\n", 1};
}

inline Instruction subi(Register r, uint32_t v) {
    return unsafe_cmpi(r, v);
}

inline Instruction subr(Register r1, Register r2) {
    return unsafe_cmpr(r1, r2);
}

// Helper function to compare registers without modifying r1
inline Instruction safe_cmpr(Register r1, Register r2) {
    return compose(
        pushr(r1),
        unsafe_cmpr(r1, r2),
        popr(r1)
    );
}

inline Instruction safe_cmpi(Register r1, int32_t i) {
    return compose(
        pushr(r1),
        unsafe_cmpi(r1, i),
        popr(r1)
    );
}

inline Instruction negr(Register r) {
    Register util_reg = static_cast<int>(r) == 10 ?
        Register::R9 : Register::R10;
    return compose(
        pushr(util_reg),
        subr(util_reg, util_reg),
        subr(util_reg, r),
        movr(r, util_reg),
        popr(util_reg)
    );
}

inline Instruction unsafe_skip_if(Register r1, Register r2, Conditional cond, Instruction do_this) {
    
    auto jump_instr = ([&]() {
        int32_t addr = static_cast<int32_t>(do_this.size() + INSTRUCTION_SIZE);
        switch (cond) {
            case Conditional::GT: return jgti(addr);
            case Conditional::LT: return jlti(addr);
            case Conditional::EQ: return jei(addr);
        }
        __builtin_unreachable();
    })();

    return compose(
        unsafe_cmpr(r1, r2),
        jump_instr,
        do_this
    );
}

inline Instruction unsafe_skip_if(Register r, int32_t v, Conditional cond, Instruction do_this) {
    
    auto jump_instr = ([&]() {
        int32_t addr = static_cast<int32_t>(do_this.size() + INSTRUCTION_SIZE);
        switch (cond) {
            case Conditional::GT: return jgti(addr);
            case Conditional::LT: return jlti(addr);
            case Conditional::EQ: return jei(addr);
        }
        __builtin_unreachable();
    })();

    return compose(
        unsafe_cmpi(r, v),
        jump_instr,
        do_this
    );
}

inline Instruction safe_skip_if(Register r1, Register r2, Conditional cond, Instruction do_this) {
    return compose(
        pushr(r1),
        unsafe_skip_if(r1, r2, cond, do_this),
        popr(r1)
    );
}

inline Instruction safe_skip_if(Register r, int32_t v, Conditional cond, Instruction do_this) {
    return compose(
        pushr(r),
        unsafe_skip_if(r, v, cond, do_this),
        popr(r)
    );
}

// Unsafe because it uses unsafe_cmpi(r, v) for comparison, which subs v from r.
inline Instruction unsafe_do_while(Register r, int32_t v, Conditional cond, Instruction do_this) {
    auto jump_instr = ([&]() {
        int32_t addr = -static_cast<int32_t>(do_this.size() + INSTRUCTION_SIZE);
        switch (cond) {
            case Conditional::GT: return jgti(addr);
            case Conditional::LT: return jlti(addr);
            case Conditional::EQ: return jei(addr);
        }
        __builtin_unreachable();
    })();

    return compose(
        do_this,
        unsafe_cmpi(r, v),
        jump_instr
    );
}

inline Instruction safe_do_while(Register r, int32_t v, Conditional cond, Instruction do_this) {
    return compose(
        pushr(r),
        unsafe_do_while(r, v, cond, do_this),
        popr(r)
    );
}

// R9  - Accumulator
// R10 - Negation flag 
// LHS - Seed (Also product, once finished)
// RHS - Counter (will be cleared out)
inline Instruction unsafe_multr(Register r1, Register r2_counter) {
    assert(r1 != Register::R9 && r1 != Register::R10);
    assert(r2_counter != Register::R9 && r2_counter != Register::R10);

    return compose(
        movi(Register::R9,  0),
        movi(Register::R10, 0),
        // unsafe skip_if OK here because it's comparing against 0.
        // If R9 is checked, then we negate it again afterwards. 
        // We do this because the counter subtracts when accumulating 
        // so it prevents an infinite loop
        unsafe_skip_if(r2_counter, Register::R9, Conditional::GT, compose(
            negr(r2_counter),
            incr(Register::R10)
        )),
        // OK to use unsafe_do_while here because 
        unsafe_do_while(r2_counter, 0, Conditional::GT, compose(
            addr(Register::R9, r1),
            addi(r2_counter, -1)
        )),
        unsafe_skip_if(Register::R10, 0, Conditional::EQ, compose(
            negr(Register::R9)
        )),
        movr(r1, Register::R9)
    );
}

