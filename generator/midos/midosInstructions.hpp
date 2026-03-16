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

// **Uses R9 and R10.**
inline Instruction unsafe_multr(Register r1, Register r2) {
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
        unsafe_do_while(counter, 0, Conditional::GT, compose(
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
inline Instruction unsafe_div(Register r1, Register r2) {
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
        unsafe_skip_if(accumulator, 0, Conditional::GT, compose(
            negr(accumulator),
            incr(lhs_is_neg)
        )),
        unsafe_skip_if(seed, 0, Conditional::GT, compose(
            negr(seed),
            incr(rhs_is_neg)
        )),
        unsafe_do_while(accumulator, 0, Conditional::GT, compose(
            subr(accumulator, seed),
            incr(quotient)
        )),
        unsafe_skip_if(accumulator, 0, Conditional::EQ, compose(
            subi(quotient, 1),
            addr(accumulator, seed)
        )),
        safe_skip_if(lhs_is_neg, rhs_is_neg, Conditional::EQ, negr(quotient)),
        unsafe_skip_if(lhs_is_neg, 0, Conditional::EQ, negr(accumulator)),
        movr(r2, accumulator),
        movr(r1, quotient)
    );
}

inline Instruction safe_multr(Register r1, Register r2) {
    return compose(
        pushr(r2),
        pushr(Register::R9),
        pushr(Register::R10),
        unsafe_multr(r1, r2),
        popr(Register::R10),
        popr(Register::R9),
        popr(r2)
    );
}

inline Instruction safe_div(Register r1, Register r2) {
    return compose(
        pushr(Register::R8),
        pushr(Register::R9),
        pushr(Register::R10),
        unsafe_div(r1, r2),
        popr(Register::R10),
        popr(Register::R9),
        popr(Register::R8)
    );
}
