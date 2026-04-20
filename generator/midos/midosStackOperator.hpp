#pragma once
#include "../stackOperator.hpp"
#include "midosInstructions.hpp"
#include <cmath>
#include <cstdint>

template<typename Generator>
struct midosStackOperator : StackOperator<Generator> {

    inline static auto midosAdder = []() {
        BaseBinary<Generator> b;
        b.handle_const_fold = std::plus<uint64_t>();
        b.handle_dreg_sreg = [](auto dst, auto src) { return MidOs::addr(dst, src); };
        b.handle_dreg_imm  = [](auto dst, uint64_t v) { return MidOs::addi(dst, static_cast<int32_t>(v)); };
        b.is_commutative = true;
        return b;
    }();

    inline static auto midosSubber = []() {
        BaseBinary<Generator> b;
        b.handle_const_fold = std::minus<uint64_t>();
        b.handle_dreg_sreg = [](auto dst, auto src) { return MidOs::subr(dst, src); };
        b.handle_dreg_imm  = [](auto dst, uint64_t v) { return MidOs::addi(dst, -static_cast<int32_t>(v)); };
        b.is_commutative = false;
        return b;
    }();

    inline static auto midosMulter = []() {
        BaseBinary<Generator> b;
        b.handle_const_fold = std::multiplies<uint64_t>();
        b.handle_dreg_sreg = [](auto dst, auto src) { return MidOs::multr(dst, src); };
        b.is_commutative = true;
        return b;
    }();

    inline static auto midosDiver = []() {
        BaseBinary<Generator> b;
        b.handle_const_fold = std::divides<uint64_t>();
        // divr puts quotient in dst, remainder in src
        b.handle_dreg_sreg = [](auto dst, auto src) { return MidOs::divr(dst, src); };
        b.is_commutative = false;
        return b;
    }();

    inline static auto midosModer = []() {
        BaseBinary<Generator> b;
        b.handle_const_fold = std::modulus<uint64_t>();
        // divr puts quotient in dst, remainder in src; move remainder into dst
        b.handle_dreg_sreg = [](auto dst, auto src) {
            return MidOs::compose(MidOs::divr(dst, src), MidOs::movr(dst, src));
        };
        b.is_commutative = false;
        return b;
    }();

    inline static auto midosExper = []() {
        BaseBinary<Generator> b;
        b.handle_const_fold = [](uint64_t l, uint64_t r) {
            return static_cast<uint64_t>(std::pow(l, r));
        };
        b.handle_dreg_sreg = [](auto dst, auto src) { return MidOs::expr(dst, src); };
        b.is_commutative = false;
        return b;
    }();

    static BaseComparison<Generator> make_comparer() {
        BaseComparison<Generator> b;
        b.handle_r_r   = [](auto lhs, auto rhs) { return MidOs::compare(lhs, rhs); };
        b.handle_r_imm = [](auto lhs, uint64_t v) { return MidOs::compare(lhs, static_cast<int32_t>(v)); };
        return b;
    }
};
