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

    void push_value(x86::Register r, uint32_t v) {
        prog_fn = x86::compose(
            std::move(prog_fn),
            x86::push(v)
        );
    }

    void neg() {
        // emit(x86::compose(
        //     // x86::popr(x86::Register::R1),
        //     // x86::negr(x86::Register::R1),
        //     // x86::pushr(x86::Register::R1)
        // ));
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
        // emit(x86::compose(
        //     // x86::popr(x86::Register::R2),
        //     // x86::popr(x86::Register::R1),
        //     // x86::subr(x86::Register::R1, x86::Register::R2),
        //     // x86::pushr(x86::Register::R1)
        // ));
    }

    void mult() {
        // emit(x86::compose(
        //     // x86::popr(x86::Register::R2),
        //     // x86::popr(x86::Register::R1),
        //     // x86::unsafe_multr(x86::Register::R1, x86::Register::R2),
        //     // x86::pushr(x86::Register::R1)
        // ));
    }

    void div () {
        // emit(x86::compose(
        //     // x86::popr(x86::Register::R2),
        //     // x86::popr(x86::Register::R1),
        //     // x86::unsafe_divr(x86::Register::R1, x86::Register::R2),
        //     // x86::pushr(x86::Register::R1)
        // ));
    }

    void mod() {
    //    emit(x86::compose(
    //     //    x86::popr(x86::Register::R2),
    //     //    x86::popr(x86::Register::R1),
    //     //    x86::unsafe_divr(x86::Register::R1, x86::Register::R2),
    //     //    x86::pushr(x86::Register::R2)
    //    ));
    }

    void exp() {
        // emit(x86::compose(
        //     // x86::popr(x86::Register::R2),
        //     // x86::popr(x86::Register::R1),
        //     // x86::unsafe_expr(x86::Register::R1, x86::Register::R2),
        //     // x86::pushr(x86::Register::R1)
        // ));
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