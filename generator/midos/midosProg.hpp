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

    void emit(const MidOs::Instruction& block) {
        block(emitter);
    }

    // Arithmetic expressions - uses a pure stack machine approach
    // by popping the top 2 recent values, calculating, then putting
    // it back into the stack.

    void push_value(MidOs::Register r, uint32_t v) {
        emit(MidOs::compose(
            MidOs::movi(r, v),
            MidOs::pushr(r)
        ));
    }

    void neg() {
        emit(MidOs::compose(
            MidOs::popr(MidOs::Register::R1),
            MidOs::negr(MidOs::Register::R1),
            MidOs::pushr(MidOs::Register::R1)
        ));
    }

    void add() {
        emit(MidOs::compose(
            MidOs::popr(MidOs::Register::R2),
            MidOs::popr(MidOs::Register::R1),
            MidOs::addr(MidOs::Register::R1, MidOs::Register::R2),
            MidOs::pushr(MidOs::Register::R1)
        ));
    }

    void sub() {
        emit(MidOs::compose(
            MidOs::popr(MidOs::Register::R2),
            MidOs::popr(MidOs::Register::R1),
            MidOs::subr(MidOs::Register::R1, MidOs::Register::R2),
            MidOs::pushr(MidOs::Register::R1)
        ));
    }

    void mult() {
        emit(MidOs::compose(
            MidOs::popr(MidOs::Register::R2),
            MidOs::popr(MidOs::Register::R1),
            MidOs::unsafe_multr(MidOs::Register::R1, MidOs::Register::R2),
            MidOs::pushr(MidOs::Register::R1)
        ));
    }

    void div () {
        emit(MidOs::compose(
            MidOs::popr(MidOs::Register::R2),
            MidOs::popr(MidOs::Register::R1),
            MidOs::unsafe_divr(MidOs::Register::R1, MidOs::Register::R2),
            MidOs::pushr(MidOs::Register::R1)
        ));
    }

    void mod() {
       emit(MidOs::compose(
           MidOs::popr(MidOs::Register::R2),
           MidOs::popr(MidOs::Register::R1),
           MidOs::unsafe_divr(MidOs::Register::R1, MidOs::Register::R2),
           MidOs::pushr(MidOs::Register::R2)
       ));
    }

    void exp() {
        emit(MidOs::compose(
            MidOs::popr(MidOs::Register::R2),
            MidOs::popr(MidOs::Register::R1),
            MidOs::unsafe_expr(MidOs::Register::R1, MidOs::Register::R2),
            MidOs::pushr(MidOs::Register::R1)
        ));
    }

};
