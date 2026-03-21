#pragma once
#include "../baseInstruction.hpp"
#include <cstdint>
#include <functional>
#include <limits>
#include <utility>

struct x86 : InstructionControl<x86> {

    enum class Register {
        EAX = 0, 
        ECX, EDX, EBX, ESP, EBP, 
        ESI, EDI, R8,  R9,  R10, 
        R11, R12, R13, R14, R15
    };

    enum class OpcodeExtension {
        Zero = 0, One, Two, Three, 
        Four, Five, Six, Seven
    };

    enum class Conditional {
        GT, GTE, LT, LTE, EQ, NEQ 
    };

    struct Instruction : BaseInstruction {
        std::function<void(unsigned char*, int&)> write_bytes = 
            [](auto* prog, auto& ptr) {};
    };



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

private:

    static Instruction compare(Register r1, Register r2) {
        return cmp(r1, r2);
    }

    static Instruction compare(Register r, int32_t v) {
        return cmp(r, v);
    }

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

    static std::string get_register(Register r) {
        return [r]() -> std::string {
            switch(r) {
                case Register::EAX: return "EAX";
                case Register::ECX: return "ECX";
                case Register::EDX: return "EDX";
                case Register::EBX: return "EBX";
                case Register::ESP: return "ESP";
                case Register::EBP: return "EBP";
                case Register::ESI: return "ESI";
                case Register::EDI: return "EDI";
                default: return "r" + std::to_string(static_cast<uint8_t>(r));
            }
        }();
    }

    static uint8_t get_rm_byte(Register r, OpcodeExtension ext) {
        return static_cast<uint8_t>(0xC0 | 
             ((static_cast<uint8_t>(r) & 0x7) | 
               static_cast<uint8_t>(ext) << 3));
    }

    static uint8_t get_rm_byte(Register rm, Register r) {
        return get_rm_byte(rm, static_cast<OpcodeExtension>(r));
    }

    template <typename... Args>
    static Instruction create_instr(std::string emit, Args&&... args) {
        return {emit, sizeof...(args), [=](auto* prog, auto& ptr) {
            ((prog[ptr++] = args), ...);
        }};
    }

    static Instruction write_rex_prefix(Register rm, Register r, bool is_64) {
        uint8_t is_w = static_cast<uint8_t>(is_64) << 3;
        uint8_t is_r = static_cast<uint8_t>(r  >= Register::R8) << 2;
        uint8_t is_b = static_cast<uint8_t>(rm >= Register::R8);
        
        return create_instr("", 0x40 | is_r | is_b | is_w);
    }

    static Instruction write_rex_prefix(Register rm, OpcodeExtension ext, bool is_64) {
        return write_rex_prefix(rm, static_cast<Register>(ext), is_64);
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

    static Instruction i(std::string opcode, int32_t v, uint8_t short_op_hex, uint8_t long_op_hex) {
        auto emit = opcode + " " + std::to_string(v) + "\n";

        if (std::in_range<int8_t>(v)) {
            return compose(
                create_instr(emit, short_op_hex),
                create_instr("", static_cast<int8_t>(v))
            );
        }

        return compose(
            create_instr(emit, long_op_hex),
            write_32(static_cast<uint32_t>(v))
        );
    }

    static Instruction rm(std::string opcode, Register r, OpcodeExtension ext, uint8_t op_hex, bool is_64 = false) {
        auto emit = opcode + " " + get_register(r) + "\n";
        auto byte = get_rm_byte(r, ext);

        if (r < Register::R8 && !is_64) {
            return create_instr(emit, op_hex, byte);
        }

        return compose(
            write_rex_prefix(r, ext, is_64),
            create_instr(emit, op_hex, byte)
        );
    }

    static Instruction rm_r(std::string opcode, Register rm, Register r, uint8_t op_hex, bool is_64 = false) {
        auto emit = opcode + " " + get_register(rm) + ", " + get_register(r) + "\n";
        auto byte = get_rm_byte(rm, r);

        if (r < Register::R8 && rm < Register::R8 && !is_64) {
            return create_instr(emit, op_hex, byte);
        }

        return compose(
            write_rex_prefix(rm, r, is_64),
            create_instr(emit, op_hex, byte)
        );
    }

    static Instruction r_rm(std::string opcode, Register r, Register rm, uint8_t op_hex, bool is_64 = false) {
        auto instr = rm_r(opcode, rm, r, op_hex, is_64);
        instr.emitted_content = opcode + " " + get_register(r) + ", " + get_register(rm) + "\n";
        return instr;
    }

    static Instruction rm_i(std::string opcode, Register r, OpcodeExtension ext, uint8_t short_op_hex, uint8_t long_op_hex, uint32_t v, bool is_64 = false) {
        if (std::in_range<int8_t>(v)) {
            return compose(
                rm(opcode, r, ext, short_op_hex, is_64),
                create_instr("", static_cast<uint8_t>(v))
            );
        }
        
        return compose(
            rm(opcode, r, ext, long_op_hex, is_64),
            write_32(v)
        );
    }

    static Instruction r_plus_rm(std::string opcode, Register r, OpcodeExtension ext, uint8_t op_hex) {
        return rm(opcode, r, ext, op_hex + static_cast<uint8_t>(r));
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

    static Instruction ret() { return create_instr("RET\n", 0xc3); }

    static Instruction push(int32_t v) { return i("PUSH", v, 0x6A, 0x68); }

    static Instruction pop(Register r) { return rm("POP", r, OpcodeExtension::Zero, 0x8F); }
    static Instruction push(Register r) { return rm("PUSH", r, OpcodeExtension::Six, 0xFF); }

    static Instruction cmp(Register r, int32_t v) { 
        return rm_i("CMP", r, OpcodeExtension::Seven, 0x83, 0x81, v); }
    static Instruction add(Register r, int32_t v) { 
        return rm_i("ADD", r, OpcodeExtension::Zero,  0x83, 0x81, v); }
    
    static Instruction cmp(Register r1, Register r2) { return rm_r("CMP", r1, r2, 0x39); }
    static Instruction add(Register r1, Register r2) { return rm_r("ADD", r1, r2, 0x01); }
};