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
            x86::imul(x86::Register::EAX),
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

    void pop_last_into_bp(int32_t offset) {
        prog_fn = x86::compose(
            std::move(prog_fn),
            x86::pop(x86::Register::EBP, offset)
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