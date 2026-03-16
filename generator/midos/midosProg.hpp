#pragma once

#include <cstdint>
#include <cstdio>
#include <utility>
#include <ostream>
#include "midosInstructions.hpp"

struct midOsProg {

private:

    std::ostream& emitter;

public:

    midOsProg(std::ostream& out) : emitter(out) {}

    void emit(const Instruction& block) {
        block(emitter);
    }

    // Arithmetic expressions - uses a pure stack machine approach
    // by popping the top 2 recent values, calculating, then putting
    // it back into the stack.

    void push_value(Register r, uint32_t v) {
        emit(compose(
            movi(r, v),
            pushr(r)
        ));
    }

    void add() {
        emit(compose(
            popr(Register::R2),
            popr(Register::R1),
            addr(Register::R1, Register::R2),
            pushr(Register::R1)
        ));
    }

    void sub() {
        emit(compose(
            popr(Register::R2),
            popr(Register::R1),
            subr(Register::R1, Register::R2),
            pushr(Register::R1)
        ));
    }

    void mult() {
        emit(compose(
            popr(Register::R2),
            popr(Register::R1),
            unsafe_multr(Register::R1, Register::R2),
            pushr(Register::R1)
        ));
    }

    void div () {
        emit(compose(
            popr(Register::R2),
            popr(Register::R1),
            unsafe_div(Register::R1, Register::R2),
            pushr(Register::R1)
        ));
    }

    void mod() {
       emit(compose(
           popr(Register::R2),
           popr(Register::R1),
           unsafe_div(Register::R1, Register::R2),
           pushr(Register::R2)
       ));
    }

    void neg() {
        emit(compose(
            popr(Register::R1),
            negr(Register::R1),
            pushr(Register::R1)
        ));
    }

};
