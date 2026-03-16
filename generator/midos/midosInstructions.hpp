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

// **Warning: Modifies flags. Alias for unsafe_cmpi(r1, v)**
inline Instruction subi(Register r, uint32_t v) {
    return unsafe_cmpi(r, v);
}

// **Warning: Modifies flags. Alias for unsafe_cmpr(r1, r2)**
inline Instruction subr(Register r1, Register r2) {
    return unsafe_cmpr(r1, r2);
}

// Helper function to compare registers without modifying r1
inline Instruction cmpr(Register r1, Register r2) {
    return compose(
        pushr(r1),
        unsafe_cmpr(r1, r2),
        popr(r1)
    );
}

inline Instruction cmpi(Register r1, int32_t i) {
    return compose(
        pushr(r1),
        unsafe_cmpi(r1, i),
        popr(r1)
    );
}

inline Instruction negr(Register r) {
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

inline Instruction skip_if(Register r1, Register r2, Conditional cond, Instruction do_this) {

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
        cmpr(r1, r2),
        jump_instr,
        do_this
    );
}

inline Instruction skip_if(Register r, int32_t v, Conditional cond, Instruction do_this) {
    
    // Check cond here because cmpi is a composite instruction. You cannot
    // rely on INSTRUCTION_SIZE because cmpi is not a real instruction.
    auto check_cond = cmpi(r, v); 

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
        check_cond,
        jump_instr,
        do_this
    );
}

// Unsafe because it uses unsafe_cmpi(r, v) for comparison, which subs v from r.
// This means that if v != 0, you probably don't want to use this.
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

inline Instruction do_while(Register r, int32_t v, Conditional cond, Instruction do_this) {
    
    auto check_cond = cmpi(r, v);
    
    auto jump_instr = ([&]() {
        int32_t addr = -static_cast<int32_t>(do_this.size() + check_cond.size());
        switch (cond) {
            case Conditional::GT: return jgti(addr);
            case Conditional::LT: return jlti(addr);
            case Conditional::EQ: return jei(addr);
        }
        __builtin_unreachable();
    })();

    return compose(
        do_this,
        check_cond,
        jump_instr
    );
}

// Uses unsafe_cmpr(r1, r2) - which subtracts r2 from r1
inline Instruction unsafe_while(Register r1, Register r2, Conditional cond, Instruction do_this) {
    
    auto jump_back = ([&]() {
        int32_t addr = -static_cast<int32_t>(do_this.size() + INSTRUCTION_SIZE);
        switch (cond) {
            case Conditional::GT: return jgti(addr);
            case Conditional::LT: return jlti(addr);
            case Conditional::EQ: return jei(addr);
        }
        __builtin_unreachable();
    })();

    return compose(
        // skips the initial pass, in the event that it doesn't satisfy
        // the cond (other wise this would be a do while loop)
        jmpi(static_cast<int32_t>(do_this.size() + INSTRUCTION_SIZE)),
        do_this,
        unsafe_cmpr(r1, r2),
        jump_back
    );
}

// Uses unsafe_cmpr(r1, v) - which subtracts v from r
inline Instruction unsafe_while(Register r, int32_t v, Conditional cond, Instruction do_this) {
        
    auto jump_back = ([&]() {
        int32_t addr = -static_cast<int32_t>(do_this.size() + INSTRUCTION_SIZE);
        switch (cond) {
            case Conditional::GT: return jgti(addr);
            case Conditional::LT: return jlti(addr);
            case Conditional::EQ: return jei(addr);
        }
        __builtin_unreachable();
    })();

    return compose(
        // skips the initial pass, in the event that it doesn't satisfy
        // the cond (other wise this would be a do while loop)
        jmpi(static_cast<int32_t>(do_this.size() + INSTRUCTION_SIZE)),
        do_this,
        unsafe_cmpi(r, v),
        jump_back
    );
}

inline Instruction _while(Register r1, Register r2, Conditional cond, Instruction do_this) {
    
    // Check cond here because cmpr is a composite instruction. You cannot
    // rely on INSTRUCTION_SIZE because cmpr is not a real instruction.
    auto check_cond = cmpr(r1, r2); 

    auto jump_back = ([&]() {
        int32_t addr = -static_cast<int32_t>(do_this.size() + check_cond.size());
        switch (cond) {
            case Conditional::GT: return jgti(addr);
            case Conditional::LT: return jlti(addr);
            case Conditional::EQ: return jei(addr);
        }
        __builtin_unreachable();
    })();

    return compose(
        // skips the initial pass, in the event that it doesn't satisfy
        // the cond (otherwise this would be a do while loop)
        jmpi(static_cast<int32_t>(do_this.size() + INSTRUCTION_SIZE)),
        do_this,
        check_cond,
        jump_back
    );
}

inline Instruction _while(Register r, int32_t v, Conditional cond, Instruction do_this) {

    // Check cond here because cmpi is a composite instruction. You cannot
    // rely on INSTRUCTION_SIZE because cmpr is not a real instruction.
    auto check_cond = cmpi(r, v);

    auto jump_back = ([&]() {
        int32_t addr = -static_cast<int32_t>(do_this.size() + check_cond.size());
        switch (cond) {
            case Conditional::GT: return jgti(addr);
            case Conditional::LT: return jlti(addr);
            case Conditional::EQ: return jei(addr);
        }
        __builtin_unreachable();
    })();

    return compose(
        // skips the initial pass, in the event that it doesn't satisfy
        // the cond (otherwise this would be a do while loop)
        jmpi(static_cast<int32_t>(do_this.size() + INSTRUCTION_SIZE)),
        do_this,
        check_cond,
        jump_back
    );
}

// **Uses R2, R9 and R10.**
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
        _while(counter, 0, Conditional::GT, compose(
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
inline Instruction unsafe_divr(Register r1, Register r2) {
    Register accumulator = r1;
    Register seed = r2;
    Register lhs_is_neg = Register::R8;
    Register rhs_is_neg = Register::R9;
    Register quotient = Register::R10;

    assert(r1 != lhs_is_neg && r1 != rhs_is_neg && r1 != quotient && r1 != quotient);
    assert(r2 != lhs_is_neg && r2 != rhs_is_neg && r2 != quotient && r2 != quotient);

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
        skip_if(lhs_is_neg, rhs_is_neg, Conditional::EQ, negr(quotient)),
        unsafe_skip_if(lhs_is_neg, 0, Conditional::EQ, negr(accumulator)),
        movr(r2, accumulator),
        movr(r1, quotient)
    );
}

// ** Uses R2, R7, R8, R9, and R10 **
inline Instruction unsafe_expr(Register r1, Register r2) {
    Register accumulator = r1;
    Register exp_counter = r2;
    Register base = Register::R7;

    assert(r1 != base && r2 != base);

    return compose(
        movr(base, accumulator),
        movi(accumulator, 1),
        _while(exp_counter, 0, Conditional::GT, compose(
            // unsafe_multr clobbers R2, so pushing and popping is necessary here.
            pushr(base),
            unsafe_multr(accumulator, base),
            popr(base),
            subi(exp_counter, 1)
        ))
    );
}

inline Instruction expr(Register r1, Register r2) {
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


inline Instruction multr(Register r1, Register r2) {
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

inline Instruction divr(Register r1, Register r2) {
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
