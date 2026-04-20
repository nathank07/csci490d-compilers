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

};
