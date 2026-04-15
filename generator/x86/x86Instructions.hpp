#pragma once
#include "../baseInstruction.hpp"
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <iostream>

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
            [](auto*, auto&) {};
    };

private:
    template <typename... Args>
    static Instruction create_instr(std::string emit, Args&&... args) {
        return {emit, sizeof...(args), [=](auto* prog, auto& ptr) {
            ((prog[ptr++] = static_cast<unsigned char>(args)), ...);
        }};
    }

    template <typename... Args>
    static Instruction create_abs_addr_instr(std::string emit, uint64_t abs_from_0x0, Args&&... args) {
        auto instr = create_instr(emit, args...);
        return {emit, instr.byte_size + 8, [instr, abs_from_0x0](auto* prog, auto& ptr) {
            instr.write_bytes(prog, ptr);
            write_64(reinterpret_cast<uint64_t>(prog) + abs_from_0x0).write_bytes(prog, ptr);
        }};
    }

public:

    static Instruction write_raw_string(std::u8string string) {
        auto emit = std::string("raw string <") + reinterpret_cast<const char*>(string.c_str()) + ">\n";
        auto in = create_instr(emit, string[0]);

        for (std::size_t i = 1; i < string.size(); i++) {
            in = x86::compose(
                std::move(in),
                create_instr("", string[i])
            );
        }

        return in;
    }

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

private:

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
               (static_cast<uint8_t>(ext) & 0x7) << 3));
    }

    static uint8_t get_rm_byte(Register rm, Register r) {
        return get_rm_byte(rm, static_cast<OpcodeExtension>(r));
    }

    static uint8_t get_rm_byte(Register r, OpcodeExtension ext, uint32_t offset) {
        uint8_t mod;

        // EBP in the r/m field with mod=00 means disp32 (table 2-2)
        // so we have to make it [EBP + disp8 (0)] by letting it be mod 01
        bool is_ebp_like = (static_cast<uint8_t>(r) & 0x7) == (static_cast<uint8_t>(Register::EBP) & 0x7);

        if (offset == 0 && !is_ebp_like) {
            mod = 0x0;
        } else if (std::in_range<uint8_t>(offset)) {
            mod = 0x40;
        } else {
            mod = 0x80;
        }

        return static_cast<uint8_t>(mod | ((static_cast<uint8_t>(r) & 0x7) |
               (static_cast<uint8_t>(ext) & 0x7) << 3));
    }

    static uint8_t get_rm_byte(Register rm, Register r, uint32_t offset) {
        return get_rm_byte(rm, static_cast<OpcodeExtension>(r), offset);
    }

    static Instruction write_rex_prefix(Register rm, Register r, bool is_64) {
        uint8_t is_w = static_cast<uint8_t>(static_cast<uint8_t>(is_64) << 3);
        uint8_t is_r = static_cast<uint8_t>(static_cast<uint8_t>(r  >= Register::R8) << 2);
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

    static Instruction write_64(uint64_t addr) {
        return compose(
            write_32(static_cast<uint32_t>(addr)),
            write_32(static_cast<uint32_t>(addr >> 32))
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

    static Instruction i(std::string opcode, int32_t v, uint8_t long_op_hex) {
        auto emit = opcode + " " + std::to_string(v) + "\n";

        return compose(
            create_instr(emit, long_op_hex),
            write_32(static_cast<uint32_t>(v))
        );
    }

    static Instruction rm(std::string opcode, Register r, OpcodeExtension ext, uint8_t op_hex, bool is_64 = true) {
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

    static Instruction rm(std::string opcode, Register r, OpcodeExtension ext, uint8_t op_hex, int32_t offset, bool is_64 = true) {
        auto emit = opcode + " [" + get_register(r) + " + " + std::to_string(offset) + "]\n";
        auto byte = get_rm_byte(r, ext, offset);

        bool is_ebp_like = (static_cast<uint8_t>(r) & 0x7) == (static_cast<uint8_t>(Register::EBP) & 0x7);

        auto write = [&](int32_t& off) {

            if (off == 0 && !is_ebp_like) {
                return compose();
            }

            // EBP in the r/m field with mod=00 means disp32 (table 2-2)
            // so we have to make it [EBP + 0]
            if (off == 0 && is_ebp_like) {
                return create_instr("", uint8_t{0});
            }

            if (std::in_range<uint8_t>(off)) {
                return create_instr("", off);
            }

            return write_32(off);
        };

        if (r < Register::R8 && !is_64) {
            return compose(
                create_instr(emit, op_hex, byte),
                write(offset)
            );
        }

        return compose(
            write_rex_prefix(r, ext, is_64),
            create_instr(emit, op_hex, byte),
            write(offset)
        );
    }

    static Instruction rm_r(std::string opcode, Register rm, Register r, uint8_t op_hex, bool is_64 = true) {
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

    static Instruction r_rm(std::string opcode, Register r, Register rm, uint8_t op_hex, bool is_64 = true) {
        auto instr = rm_r(opcode, rm, r, op_hex, is_64);
        instr.emitted_content = opcode + " " + get_register(r) + ", " + get_register(rm) + "\n";
        return instr;
    }

    static Instruction rm_i(std::string opcode, Register r, OpcodeExtension ext, uint8_t short_op_hex, uint8_t long_op_hex, uint32_t v, bool is_64 = true) {        
        if (std::in_range<int8_t>(v)) {
            return compose(
                rm(opcode + " [, " + std::to_string(v) + "]", r, ext, short_op_hex, is_64),
                create_instr("", static_cast<uint8_t>(v))
            );
        }
        
        return rm_i32(opcode, r, ext, long_op_hex, v, is_64);
    }

    static Instruction rm_i32(std::string opcode, Register r, OpcodeExtension ext, uint8_t op_hex, uint32_t v, bool is_64 = true) {
        opcode = opcode + " [, " + std::to_string(v) + "]";
        
        return compose(
            rm(opcode, r, ext, op_hex, is_64),
            write_32(v)
        );
    }

    static Instruction rm_i64(std::string opcode, Register r, OpcodeExtension ext, uint8_t op_hex, uint32_t v, bool is_64 = true) {
        opcode = opcode + " [, " + std::to_string(v) + "]";
        
        return compose(
            rm(opcode, r, ext, op_hex, is_64),
            write_64(v)
        );
    }

    static Instruction r_plus_rm(std::string opcode, Register r, OpcodeExtension ext, uint8_t op_hex) {
        return rm(opcode, r, ext, (op_hex + static_cast<uint8_t>(r)) & 0x7);
    }

    // End utils

public: 

    // x86 instructions

    static Instruction je  (int32_t addr) { return jcc("JE",  addr, 0x74); }
    static Instruction jne (int32_t addr) { return jcc("JNE", addr, 0x75); }
    static Instruction jg  (int32_t addr) { return jcc("JG",  addr, 0x7F); }
    static Instruction jge (int32_t addr) { return jcc("JGE", addr, 0x7D); }
    static Instruction jl  (int32_t addr) { return jcc("JL",  addr, 0x7C); }
    static Instruction jle (int32_t addr) { return jcc("JLE", addr, 0x7E); }

    static Instruction ret() { return create_instr("RET\n", 0xc3); }
    static Instruction cdq() { return create_instr("CDQ\n", 0x99); }
        
    static Instruction imul(Register r1, Register r2) {
        return compose(
            create_instr("IMUL", 0x0F),
            r_rm("", r1, r2, 0xAF, false)
        );
    }

    static Instruction imul(Register r, int32_t v) {
        auto emit = "IMUL " + get_register(r) + ", " + std::to_string(v) + "\n";
        if (std::in_range<int8_t>(v)) {
            return compose(
                write_rex_prefix(r, r, true),
                create_instr(emit, 0x6B, get_rm_byte(r, r)),
                create_instr("", static_cast<uint8_t>(v))
            );
        }
        return compose(
            write_rex_prefix(r, r, true),
            create_instr(emit, 0x69, get_rm_byte(r, r)),
            write_32(static_cast<uint32_t>(v))
        );
    }

    static Instruction mov_64(Register r, uint64_t v) {
        auto emit = "MOV " + get_register(r) + ", " + std::to_string(v) + "\n";
        return compose(
            write_rex_prefix(r, OpcodeExtension::Zero, true),
            create_instr(emit, 0xB8 + (static_cast<uint8_t>(r) & 0x7)),
            write_64(v)
        );
    }

    static Instruction mov_abs(Register r, uint64_t abs_from_0x0) {
        auto emit = "MOV " + get_register(r) + ", abs: " + std::to_string(abs_from_0x0) + "\n";
        return compose(
            write_rex_prefix(r, OpcodeExtension::Zero, true),
            create_abs_addr_instr(emit, abs_from_0x0, 0xB8 + (static_cast<uint8_t>(r) & 0x7))
        );
    }

    static Instruction mov_memr_32(Register src, Register mem_addr, int32_t offset) {
        auto instr = rm("MOV", mem_addr, static_cast<OpcodeExtension>(src), 0x8B, offset);
        instr.emitted_content = "MOV " + get_register(src) + ", [" + get_register(mem_addr) + " + " + std::to_string(offset) + "]\n";
        return instr;
    }

    static Instruction mov_rmem_64(Register mem_addr, Register src, int32_t offset) {
        auto instr = rm("MOV", mem_addr, static_cast<OpcodeExtension>(src), 0x89, offset);
        instr.emitted_content = "MOV [" + get_register(mem_addr) + " + " + std::to_string(offset) + "], " + get_register(src) + "\n";
        return instr;
    }

    static Instruction mov_mem_32(Register r, int32_t imm, int32_t offset) {
        auto instr = compose(
            rm("MOV", r, OpcodeExtension::Zero, 0xC7, offset),
            write_32(static_cast<uint32_t>(imm))
        );
        instr.emitted_content = "MOV [" + get_register(r) + " + " + std::to_string(offset) + "], " + std::to_string(imm) + "\n";
        return instr;
    }

    static Instruction add_mem(Register r, int32_t offset_from_bp) {
        auto instr = rm("ADD", Register::EBP, static_cast<OpcodeExtension>(r), 0x03, offset_from_bp);
        instr.emitted_content = "ADD " + get_register(r) + ", [EBP + " + std::to_string(offset_from_bp) + "]\n";
        return instr;
    }

    static Instruction sub_mem(Register r, int32_t offset_from_bp) {
        auto instr = rm("SUB", Register::EBP, static_cast<OpcodeExtension>(r), 0x2B, offset_from_bp);
        instr.emitted_content = "SUB " + get_register(r) + ", [EBP + " + std::to_string(offset_from_bp) + "]\n";
        return instr;
    }
    
    static Instruction push(int32_t v) { return i("PUSH", v, 0x6A, 0x68); }
    static Instruction jmp(int32_t addr) { return i("JMP", addr, 0xEB, 0xE9); }
    static Instruction jmp32(int32_t addr) { return i("JMP", addr, 0xE9); }


    static Instruction inc(Register r) { return rm("INC", r, OpcodeExtension::Zero, 0xFF); }
    static Instruction neg(Register r) { return rm("NEG", r, OpcodeExtension::Three, 0xF7); }
    static Instruction pop(Register r) { return rm("POP", r, OpcodeExtension::Zero, 0x8F); }
    static Instruction push(Register r) { return rm("PUSH", r, OpcodeExtension::Six, 0xFF); }
    static Instruction imul(Register r) { return rm("IMUL", r, OpcodeExtension::Five, 0xF7); }
    static Instruction idiv(Register r) { return rm("IDIV", r, OpcodeExtension::Seven, 0xF7); }
    static Instruction sar1(Register r) { return rm("SAR [, 1]", r, OpcodeExtension::Seven, 0xD1); }
    static Instruction call(Register r) { return rm("CALL", r, OpcodeExtension::Two, 0xFF, true); }

    static Instruction cmp(Register r, int32_t v) {
        return rm_i("CMP", r, OpcodeExtension::Seven, 0x83, 0x81, v); }
    static Instruction add(Register r, int32_t v) { 
        return rm_i("ADD", r, OpcodeExtension::Zero,  0x83, 0x81, v); }
    static Instruction sub(Register r, int32_t v) { 
        return rm_i("SUB", r, OpcodeExtension::Five,  0x83, 0x81, v); }
    
    static Instruction test(Register r, int32_t v) {
        return rm_i32("TEST", r, OpcodeExtension::Zero, 0xF7, v); }
    
    static Instruction cmp(Register r1, Register r2) { return rm_r("CMP", r1, r2, 0x39); }
    static Instruction add(Register r1, Register r2) { return rm_r("ADD", r1, r2, 0x01); }
    static Instruction sub(Register r1, Register r2) { return rm_r("SUB", r1, r2, 0x29); }
    static Instruction mov(Register r1, Register r2) { return rm_r("MOV", r1, r2, 0x89); }
    static Instruction test(Register r1, Register r2) { return rm_r("TEST", r1, r2, 0x85); }
    static Instruction _xor(Register r1, Register r2) { return rm_r("XOR", r1, r2, 0x31); }

    static Instruction pop(Register r, int32_t offset) { return rm("POP", r, OpcodeExtension::Zero, 0x8F, offset); }
    static Instruction push(Register r, int32_t offset) { return rm("PUSH", r, OpcodeExtension::Six, 0xFF, offset); }

    // End x86 instructions

    // Psuedo instructions

    static Instruction exp(Register r1, Register r2) {

        Register accumulator = r1;
        Register exponent = r2;
        Register base = Register::EDI;

        while (base >= Register::EAX) {
            if (base != accumulator && base != exponent) break;
            base = static_cast<Register>(static_cast<uint8_t>(base) - 1);
        }

        return compose(
            push(base),
            mov(base, accumulator),
            _xor(accumulator, accumulator),
            x86::skip_if(exponent, 0, Conditional::LT, compose(
                inc(accumulator),
                x86::_while(exponent, 0, Conditional::NEQ, compose(
                    x86::skip_if(test(exponent, 1), Conditional::EQ, imul(accumulator, base)),
                    imul(base, base),
                    sar1(exponent)
                ))
            )),
            pop(base)
        );
    };

private:

    template <typename T>
    static void __print(T v) {
        std::cout << v;
    }
    
    static int32_t __read_int() {
        int32_t v;
        std::cin >> v;
        return v;
    }

public:

    static Instruction align_sp_start(uint64_t frame_size) {
        if (frame_size % 16 == 0) return compose();
        return sub(Register::ESP, 8);
    }

    static Instruction align_sp_end(uint64_t frame_size) {
        if (frame_size % 16 == 0) return compose();
        return add(Register::ESP, 8);
    }

    static Instruction print_num_literal(Register r) {
        assert(r != Register::ESI);

        return compose(
            mov_64(Register::ESI, reinterpret_cast<uint64_t>(__print<int32_t>)),
            mov(Register::EDI, r),
            call(Register::ESI)
        );
    }

    static Instruction print_char_addr(Register r) {
        assert(r != Register::ESI);

        return compose(
            mov_64(Register::ESI, reinterpret_cast<uint64_t>(__print<char *>)),
            mov(Register::EDI, r),
            call(Register::ESI)
        );
    }

    static Instruction read_int4(Register r, uint64_t frame_size) {
        assert(r != Register::ESI);

        auto call_read = compose(
            align_sp_start(frame_size),
            mov_64(Register::ESI, reinterpret_cast<uint64_t>(__read_int)),
            call(Register::ESI),
            align_sp_end(frame_size)
        );

        if (r == Register::EAX)
            return call_read;

        return compose(
            std::move(call_read),
            mov(r, Register::EAX)
        );
    }

    // End psuedo instructions
};