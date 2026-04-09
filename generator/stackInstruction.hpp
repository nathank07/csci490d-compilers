#pragma once
#include "stackAllocator.hpp"
#include <cassert>

template <typename Generator>
struct StackOperation {

    using Register = typename Generator::Register;
    using StackUnit = typename StackAllocator<Generator, typename Generator::Register>::StackUnit;

private:

    static auto free_space_for(Register& reg) {
        auto dest = Generator::stack.force_space_for(reg);
        if (!dest) return Generator::compose();
        return std::visit(overloads {
            [&](RegisterUnit<Register>& u) {
                reg = u.in_register;
                return Generator::compose();
            },
            [&](VirtualRegisterUnit& u) {
                return Generator::move_reg_into_sp_offset(reg, Generator::stack.get_vreg(u.sp_idx));
            }
        }, *dest);
    }

    static auto normalize_into(Register& reg, StackUnit& unit) {
        return std::visit(overloads {
            [&](IdentifierUnit& u) {
                auto offset = Generator::stack.get(u.ident);
                unit = RegisterUnit<Register>{ reg };
                return Generator::move_sp_offset_into_reg(offset, reg);
            },
            [&](VirtualRegisterUnit& u) {
                auto offset = Generator::stack.get_vreg(u.sp_idx);
                auto emit = Generator::move_sp_offset_into_reg(offset, reg);
                unit = RegisterUnit<Register>{ reg };
                return emit;
            },
            [&](RegisterUnit<Register>& u) {
                reg = u.in_register;
                return Generator::compose();
            },
            [&](ValueUnit& u) {
                return Generator::compose();
            },
            [&](StaticPointerUnit& u) {
                return Generator::compose();
            }
        }, unit);
    }

    static auto make_const_reg(Register reg, StackUnit& unit) {
        auto value = StackUtils::maybe_value_u64(unit);
        assert(value);
        unit = RegisterUnit<Register> { reg };
        return Generator::mov_const_to_reg(*value, reg);
    }

    static auto perform_binary_op(auto const_fold, auto perform_r_r, auto perform_r_imm, bool is_commutative) {

        auto rhs_reg = Generator::primary_scratch();
        auto lhs_reg = Generator::secondary_scratch();
        auto rhs = Generator::stack.pop();
        auto lhs = Generator::stack.pop();
        auto l_const = StackUtils::maybe_value_u64(lhs);
        auto r_const = StackUtils::maybe_value_u64(rhs);

        if (l_const && r_const) {
            Generator::stack.push_const(const_fold(*l_const, *r_const));
            return Generator::compose();
        }

        if (StackUtils::is_register<StackUnit, Register>(rhs, lhs_reg)) {
            std::swap(lhs_reg, rhs_reg);
        }

        auto free_lhs = free_space_for(lhs_reg);
        auto free_rhs = free_space_for(rhs_reg);
        auto norm_rhs = normalize_into(rhs_reg, rhs);
        auto norm_lhs = normalize_into(lhs_reg, lhs);

        auto emit = Generator::compose(
            std::move(free_lhs),
            std::move(free_rhs),
            std::move(norm_rhs),
            std::move(norm_lhs)
        );

        bool lhs_ok = StackUtils::is_register<StackUnit, Register>(lhs) || l_const;
        bool rhs_ok = StackUtils::is_register<StackUnit, Register>(rhs) || r_const;
        assert(lhs_ok);
        assert(rhs_ok);

        if (!is_commutative && l_const) {
            Generator::stack.push(RegisterUnit{ lhs_reg });
            return Generator::compose(
                std::move(emit),
                make_const_reg(lhs_reg, lhs),
                perform_r_r(lhs_reg, rhs_reg)
            );
        }

        if (is_commutative && l_const) {
            Generator::stack.push(RegisterUnit{ rhs_reg });
            return Generator::compose(
                std::move(emit),
                perform_r_imm(rhs_reg, *l_const)
            );
        }

        if (r_const) {
            Generator::stack.push(RegisterUnit{ lhs_reg });
            return Generator::compose(
                std::move(emit),
                perform_r_imm(lhs_reg, *r_const)
            );
        }

        Generator::stack.push(RegisterUnit{ lhs_reg });
        return Generator::compose(
            std::move(emit),
            perform_r_r(lhs_reg, rhs_reg)
        );
    }


public:

    static auto perform_unary_op(auto const_fold, auto perform_r) {
        auto reg = Generator::primary_scratch();
        auto top = Generator::stack.pop();
        auto top_const = StackUtils::maybe_value_u64(top);

        if (top_const) {
            Generator::stack.push_const(const_fold(*top_const));
            return Generator::compose();
        }

        auto free_reg = free_space_for(reg);
        auto norm_reg = normalize_into(reg, top);

        bool reg_ok = StackUtils::is_register<StackUnit, Register>(top);
        assert(reg_ok);

        Generator::stack.push(RegisterUnit{ reg });
        return Generator::compose(
            std::move(free_reg),
            std::move(norm_reg),
            perform_r(reg)
        );
    }

    static auto commutative_binary(auto const_fold, auto perform_r_r, auto perform_r_imm) {
        return perform_binary_op(
            const_fold,
            perform_r_r,
            perform_r_imm,
            true
        );
    }

    static auto non_commutative_binary(auto const_fold, auto perform_r_r, auto perform_r_imm) {
        return perform_binary_op(
            const_fold,
            perform_r_r,
            perform_r_imm,
            false
        );
    }
};