#pragma once

#include "x86Instructions.hpp"
#include <sys/mman.h>
#include <cstdio>
#include <ostream>
#include <utility>
#include <iostream>

struct x86Prog {

    x86::Instruction prog_fn = x86::compose();

    // Arithmetic expressions - uses a pure stack machine approach
    // by popping the top 2 recent values, calculating, then putting
    // it back into the stack.

    void push_value(uint32_t v) {
        prog_fn = x86::compose(
            std::move(prog_fn),
            x86::push(v)
        );
    }

    void neg() {
        prog_fn = x86::compose(
            std::move(prog_fn),
            x86::pop(x86::Register::EAX),
            x86::neg(x86::Register::EAX),
            x86::push(x86::Register::EAX)
        );
    }

    void add() {
        prog_fn = x86::compose(
            std::move(prog_fn),
            x86::pop(x86::Register::ECX),
            x86::pop(x86::Register::EAX),
            x86::add(x86::Register::EAX, x86::Register::ECX),
            x86::push(x86::Register::EAX)
        );
    }

    void sub() {
        prog_fn = x86::compose(
            std::move(prog_fn),
            x86::pop(x86::Register::ECX),
            x86::pop(x86::Register::EAX),
            x86::sub(x86::Register::EAX, x86::Register::ECX),
            x86::push(x86::Register::EAX)
        );
    }

    void mult() {
        prog_fn = x86::compose(
            std::move(prog_fn),
            x86::pop(x86::Register::ECX),
            x86::pop(x86::Register::EAX),
            x86::imul(x86::Register::ECX),
            x86::push(x86::Register::EAX)
        );
    }

    void div() {
        prog_fn = x86::compose(
            std::move(prog_fn),
            x86::pop(x86::Register::ECX),
            x86::pop(x86::Register::EAX),
            x86::cdq(),
            x86::idiv(x86::Register::ECX),
            x86::push(x86::Register::EAX)
        );
    }

    void mod() {
        prog_fn = x86::compose(
            std::move(prog_fn),
            x86::pop(x86::Register::ECX),
            x86::pop(x86::Register::EAX),
            x86::cdq(),
            x86::idiv(x86::Register::ECX),
            x86::push(x86::Register::EDX)
        );
    }

    void exp() {
        prog_fn = x86::compose(
            std::move(prog_fn),
            x86::pop(x86::Register::ECX),
            x86::pop(x86::Register::EAX),
            x86::exp(x86::Register::EAX, x86::Register::ECX),
            x86::push(x86::Register::EAX)
        );
    }

    void load_var(int32_t symbol_table_bp_offset) {
        prog_fn = x86::compose(
            std::move(prog_fn),
            x86::pop(x86::Register::EBP, symbol_table_bp_offset)
        );
    }

    void push_var(int32_t symbol_table_bp_offset) {
        prog_fn = x86::compose(
            std::move(prog_fn),
            x86::push(x86::Register::EBP, symbol_table_bp_offset)
        );
    }

    void print_num() {
        prog_fn = x86::compose(
            std::move(prog_fn),
            x86::pop64(x86::Register::EAX),
            x86::print_num_literal(x86::Register::EAX)
        );
    }

    uint64_t push_string(std::u8string str) {

        x86::Instruction str_data;
        str += '\0';
        while (str.size() % 8 != 0) str += '\0';

        for (std::size_t i = str.size(); i >= 8; i -= 8) {
            uint64_t v = 0;
            for (std::size_t j = 0; j < 8; j++) {
                v |= static_cast<uint64_t>(str[(i - 8) + j]) << (j * 8);
            }
            str_data = x86::compose(
                std::move(str_data),
                x86::mov64(x86::Register::EAX, v),
                x86::push64(x86::Register::EAX)
            );
        }
        
        prog_fn = x86::compose(
            std::move(prog_fn),
            std::move(str_data),
            x86::push64(x86::Register::ESP)
        );

        return str.size() / 8;
    }

    void print_str(uint64_t last_str_size) {

        x86::Instruction pops;

        for (uint64_t i = 0; i < last_str_size; i++) {
            pops = x86::compose(
                pops,
                x86::pop64(x86::Register::EAX)
            );
        }

        prog_fn = x86::compose(
            std::move(prog_fn),
            x86::pop64(x86::Register::EAX),
            x86::print_char_addr(x86::Register::EAX),
            std::move(pops)
        );
    }
    
    static long long run_prog(const x86Prog& p, int max_size = 50000) {
        return run_prog_bytes(p, max_size).first;
    }

    static std::pair<long long, int> run_prog_bytes(const x86Prog& p, int max_size = 50000) {
        unsigned char* prog = (unsigned char*) mmap(0, max_size,
                                PROT_EXEC | PROT_READ | PROT_WRITE,
                                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (prog == MAP_FAILED) {
            perror("mmap failed");
        }

        int ptr = 0;
        
        p.prog_fn.write_bytes(prog, ptr);

        // for (int i = 0; i < ptr; i++) {
        //     std::cout << std::hex << static_cast<int>(prog[i]) << " ";
        // }

        auto rv = ((int (*) (void)) prog) ();
        
        munmap(prog, max_size);

        return std::pair(rv, ptr);
    }

    static void emit_prog(const x86Prog& p, std::ostream& o) {
        o << p.prog_fn.emitted_content;
    }
        
};