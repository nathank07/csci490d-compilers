#pragma once

#include "x86operands.hpp"
#include <sys/mman.h>
#include <cstdio>
#include <errno.h>
#include <iostream>

struct x86Prog {
    
    unsigned char* prog;
    int ptr;

    void write32(uint32_t v) {
        prog[ptr++] = (char)(v & 0xFF);
        prog[ptr++] = (char)((v >> 8) & 0xFF);
        prog[ptr++] = (char)((v >> 16) & 0xFF);
        prog[ptr++] = (char)((v >> 24) & 0xFF);
    }

    void write_extended(char opcode, ExtendedRegister dst_r, OpcodeExtension ext) {
        // since we're not worried about width bit, this can be hardcoded
        // as 0x41 since this function is only called when dst_r >= 0x8 
        // such that (0x40 | (dst_r >> 3)) is always the same
        prog[ptr++] = 0x41;
        prog[ptr++] = opcode;
        prog[ptr++] = get_byte_32(get_lower_order(dst_r), ext);
    }

    void write_extended(char opcode, ModRM dst_r, AnyRegister src_r, char prefix = 0) {
        prog[ptr++] = 0x40 | (rex_bit(dst_r)) | (rex_bit(src_r) << 2);
        if (prefix) prog[ptr++] = prefix;
        prog[ptr++] = opcode;
        prog[ptr++] = get_byte_32_r_rm(get_lower_order(dst_r.reg), get_lower_order(src_r));
    }

    void write_extended(char opcode, AnyRegister dst_r, ModRM src_r, char prefix = 0) {
        prog[ptr++] = 0x40 | (rex_bit(src_r)) | (rex_bit(dst_r) << 2);
        if (prefix) prog[ptr++] = prefix;
        prog[ptr++] = opcode;
        prog[ptr++] = get_byte_32_rm_r(get_lower_order(dst_r), get_lower_order(src_r.reg));
    }

    void mov(Register dst_r, Immediate src_imm) {
        prog[ptr++] = 0xC7;
        prog[ptr++] = get_byte_32(dst_r, OpcodeExtension::Zero);
        write32(src_imm.value);
    }

    void mov(AnyRegister dst_r, AnyRegister src_r) {
        write_extended(0x89, ModRM{dst_r}, src_r);
    }

    void add(Register dst_r, Immediate src_imm) {
        prog[ptr++] = 0x81;
        prog[ptr++] = get_byte_32(dst_r, OpcodeExtension::Zero);
        write32(src_imm.value);
    }

    void add(Register dst_r, Register src_r) {
        prog[ptr++] = 0x01;
        prog[ptr++] = get_byte_32_r_rm(dst_r, src_r);
    }

    void sub(Register dst_r, Immediate src_imm) {
        prog[ptr++] = 0x81;
        prog[ptr++] = get_byte_32(dst_r, OpcodeExtension::Five);
        write32(src_imm.value);
    } 

    void sub(Register dst_r, Register src_r) {
        prog[ptr++] = 0x29;
        prog[ptr++] = get_byte_32_r_rm(dst_r, src_r);
    }

    void imul(Register dst_r) {
        prog[ptr++] = 0xF7;
        prog[ptr++] = get_byte_32(dst_r, OpcodeExtension::Five);
    }

    void imul(ExtendedRegister dst_r, ExtendedRegister src_r) {
        write_extended(0xAF, dst_r, ModRM{src_r}, 0x0F);
    }

    void cdq() {
        prog[ptr++] = 0x99;
    }

    void neg(Register dst_r) {
        prog[ptr++] = 0xF7;
        prog[ptr++] = get_byte_32(dst_r, OpcodeExtension::Three);
    }

    void idiv(Register dst_r) {
        prog[ptr++] = 0xF7;
        prog[ptr++] = get_byte_32(dst_r, OpcodeExtension::Seven);
    }

    void ret() {
        prog[ptr++] = 0xc3;
    }

    void push(Register r) {
        prog[ptr++] = 0xFF;
        prog[ptr++] = get_byte_32(r, OpcodeExtension::Six);
    }

    void pop(Register r) {
        prog[ptr++] = 0x8F;
        prog[ptr++] = get_byte_32(r, OpcodeExtension::Zero);
    }

    void pop(ExtendedRegister r) {
        write_extended(0x8F, r, OpcodeExtension::Zero);
    }

    void test(AnyRegister dst_r, AnyRegister src_r) {
        write_extended(0x85, ModRM{dst_r}, src_r);
    }

    void test(ExtendedRegister dst_r, Immediate src_imm) {
        write_extended(0xF7, dst_r, OpcodeExtension::Zero);
        write32(src_imm.value);
    }

    void xor_(AnyRegister dst_r, AnyRegister src_r) {
        write_extended(0x33, dst_r, ModRM{src_r});
    }

    void jl(uint8_t addr) {
        prog[ptr++] = 0x7C;
        prog[ptr++] = addr;
    }

    void je(uint8_t addr) {
        prog[ptr++] = 0x74;
        prog[ptr++] = addr;
    }
    
    void jmp(uint8_t addr) {
        prog[ptr++] = 0xEB;
        prog[ptr++] = addr;
    }

    void inc(ExtendedRegister r) {
        write_extended(0xFF, r, OpcodeExtension::Zero);
    }

    void sar1(ExtendedRegister r) {
        write_extended(0xD1, r, OpcodeExtension::Seven);
    }

    template <typename F>
    static long long run_prog(F&& instructions, int max_size = 50000) {
        unsigned char* prog = (unsigned char*) mmap(0, max_size,
                                PROT_EXEC | PROT_READ | PROT_WRITE,
                                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (prog == MAP_FAILED) {
            perror("mmap failed");
        }
        x86Prog program{prog, 0};

        instructions(program);

        // for (int i = 0; i < program.ptr; i++) {
        //     std::cout << std::hex << static_cast<int>(program.prog[i]) << " ";
        // }

        auto rv = ((int (*) (void)) prog) ();
        
        munmap(prog, max_size);

        return rv;
    }
        
};