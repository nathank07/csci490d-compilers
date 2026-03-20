#pragma once
#include "../baseInstruction.hpp"
#include "x86operands.hpp"
#include <cstdint>
#include <functional>
#include <limits>
#include <utility>

struct x86 : InstructionControl<x86> {

    enum class Conditional {
        GT, GTE, LT, LTE, EQ, NEQ 
    };

    struct Instruction : BaseInstruction {
        std::function<void(unsigned char*, int&)> write_bytes = 
            [](auto* prog, auto& ptr) {};
    };


private:

    // ** CRTP Instructions **
    static Instruction compose() { return {}; }

    template<typename... Args>
    static Instruction compose(Instruction first, Args&&... rest) {
        auto remaining = compose(std::forward<Args>(rest)...);
        first.emitted_content += remaining.emitted_content;
        first.byte_size += remaining.byte_size;
        first.write_bytes = [lhs = std::move(first.write_bytes),
                             rhs = std::move(remaining.write_bytes)]
                            (auto* prog, auto& ptr) {
            lhs(prog, ptr);
            rhs(prog, ptr);
        };
        return first;
    }

    // static Instruction compare(Register r1, Register r2) {
    //     return cmpr(r1, r2);
    // }
    // static Instruction compare(Register r, int32_t v) {
    //     return cmpi(r, v);
    // }
    static Instruction jump_rel(int32_t addr) {
        if (addr > 0) 
            return jmp(addr);

        if (addr >= std::numeric_limits<int8_t>::min() + 2) 
            return jmp(addr - 2);

        return jmp(addr - 5);
    }
    static Instruction jump_rel_when(int32_t addr, Conditional cond) {
        if (addr > 0) 
            return jmp_with_flag(addr, cond);

        if (addr >= std::numeric_limits<int8_t>::min() + 2) 
            return jmp_with_flag(addr - 2, cond);

        return jmp_with_flag(addr - 5, cond);
    }
    // ** End CRTP Instructions **

    // Utils

    template <typename... Args>
    static Instruction create_instr(std::string emit, Args&&... args) {
        return {emit, sizeof...(args), [=](auto* prog, auto& ptr) {
            ((prog[ptr++] = args), ...);
        }};
    }

    static Instruction write_32(uint32_t addr) {
        return create_instr("", 
            (uint8_t) (addr & 0xFF), 
            (uint8_t)((addr >> 8) & 0xFF), 
            (uint8_t)((addr >> 16) & 0xFF), 
            (uint8_t)((addr >> 24) & 0xFF)
        );
    }

    static Instruction jcc(std::string opcode, int32_t addr, uint8_t short_op_hex) {
        auto emit = opcode + " " + std::to_string(addr) + "\n";

        if (std::in_range<int8_t>(addr)) {
            return create_instr(emit, short_op_hex, static_cast<uint8_t>(addr));
        }

        return compose(
            create_instr(emit, 0x0F, short_op_hex + 0x10),
            write_32(addr)
        );
    }

    // End utils
    
public: 

    static Instruction jmp(int32_t addr) {

        auto emit = "JMP " + std::to_string(addr) + "\n";

        if (std::in_range<int8_t>(addr)) {
            return create_instr(emit, 0xEB, static_cast<uint8_t>(addr));
        }

        return compose(
            create_instr(emit, 0xE9),
            write_32(addr)
        );
    };

    static Instruction je  (int32_t addr) { return jcc("JE",  addr, 0x74); }
    static Instruction jne (int32_t addr) { return jcc("JNE", addr, 0x75); }
    static Instruction jg  (int32_t addr) { return jcc("JG",  addr, 0x7F); }
    static Instruction jge (int32_t addr) { return jcc("JGE", addr, 0x7D); }
    static Instruction jl  (int32_t addr) { return jcc("JL",  addr, 0x7C); }
    static Instruction jle (int32_t addr) { return jcc("JLE", addr, 0x7E); }

    static Instruction jmp_with_flag(int32_t rel_addr, Conditional on_cond) {
        switch (on_cond) {
            case Conditional::GT:  return jg(rel_addr);
            case Conditional::LT:  return jl(rel_addr);
            case Conditional::EQ:  return je(rel_addr);
            case Conditional::GTE: return jge(rel_addr);
            case Conditional::LTE: return jle(rel_addr);
            case Conditional::NEQ: return jne(rel_addr);
        }
        __builtin_unreachable();
    }
};