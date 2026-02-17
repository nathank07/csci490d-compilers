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

    void mov(Register dst_r, Immediate src_imm) {
        prog[ptr++] = 0xC7;
        prog[ptr++] = get_byte_32(dst_r, OpcodeExtension::Zero);
        write32(src_imm.value);
    }

    void add(Register dst_r, Immediate src_imm) {
        prog[ptr++] = 0x81;
        prog[ptr++] = get_byte_32(dst_r, OpcodeExtension::Zero);
        write32(src_imm.value);
    }

    void add(Register dst_r, Register src_r) {
        prog[ptr++] = 0x01;
        prog[ptr++] = get_byte_32_mr(dst_r, src_r);
    }

    void sub(Register dst_r, Immediate src_imm) {
        prog[ptr++] = 0x81;
        prog[ptr++] = get_byte_32(dst_r, OpcodeExtension::Five);
        write32(src_imm.value);
    } 

    void sub(Register dst_r, Register src_r) {
        prog[ptr++] = 0x29;
        prog[ptr++] = get_byte_32_mr(dst_r, src_r);
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

        return ((int (*) (void)) prog) ();
    }
        
};