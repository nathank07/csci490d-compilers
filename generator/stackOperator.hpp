#pragma once
#include "stackAllocator.hpp"
#include <cassert>
#include <cstdlib>
#include <iostream>

template <typename Generator>
struct StackOperator {

    StackAllocator<Generator, typename Generator::Register>& stack;

    using Register = typename Generator::Register;
    using StackUnit = typename StackAllocator<Generator, typename Generator::Register>::StackUnit;

private:

    /* 
        Evicts your desired reg and returns an instruction on what you need to
        do to save the register you evicted.
    */
    auto evict_space_for(Register desired_reg) {
        auto dest = stack.lock_reg(desired_reg);
        if (!dest) return Generator::compose();
        return std::visit(overloads {
            [&](RegisterUnit<Register>& u) {
                return Generator::move_reg_to_reg(u.in_register, desired_reg);
            },
            [&](VirtualRegisterUnit& u) {
                return Generator::move_reg_into_sp_offset(desired_reg, stack.get_vreg(u.sp_idx));
            }
        }, *dest);
    }

    /*
        Like evict_space_for but instead of evicting, it returns the first
        free scratch register. If there are no free scratch registers,
        then it will evict.
    */
    auto find_free_reg(Register& desired_reg) {
        auto dest = stack.lock_reg(desired_reg);
        if (!dest) return Generator::compose();
        return std::visit(overloads {
            [&](RegisterUnit<Register>& u) {
                if (stack.in_stack(u.in_register)) {
                    return Generator::move_reg_to_reg(u.in_register, desired_reg);
                }
                desired_reg = u.in_register;
                return Generator::compose();
            },
            [&](VirtualRegisterUnit& u) {
                return Generator::move_reg_into_sp_offset(desired_reg, stack.get_vreg(u.sp_idx));
            }
        }, *dest);
    }

    /*
        Helper that loads values into a register. This is somewhat of an opposite
        parallel to free_space_for(), and usually you want to use free_space_for()
        prior to calling normalize_into(); As this function doesn't check if the 
        register is locked and you may overwrite a register that is locked.
    */
    auto normalize_into(Register reg, StackUnit& unit) {
        return std::visit(overloads {
            [&](IdentifierUnit& u) {
                auto offset = stack.get(u.ident);
                unit = RegisterUnit<Register>{ reg };
                return Generator::move_sp_offset_into_reg(offset, reg);
            },
            [&](VirtualRegisterUnit& u) {
                auto offset = stack.get_vreg(u.sp_idx);
                unit = RegisterUnit<Register>{ reg };
                return Generator::move_sp_offset_into_reg(offset, reg);
            },
            [&](RegisterUnit<Register>& u) {
                if (u.in_register == reg) return Generator::compose();
                auto emit = Generator::move_reg_to_reg(reg, u.in_register);
                stack.unlock_reg(u.in_register);
                unit = RegisterUnit<Register>{ reg };
                return emit;
            },
            [&](ValueUnit& u) {
                auto emit = Generator::mov_const_to_reg(u.literal, reg);
                unit = RegisterUnit<Register>{ reg };
                return emit;
            },
            [&](StaticPointerUnit& u) {
                return Generator::compose();
            }
        }, unit);
    }

    auto perform_binary_op(auto const_fold, auto perform_r_r, auto perform_r_imm, bool is_commutative) {

        auto rhs_reg = Generator::primary_scratch();
        auto lhs_reg = Generator::secondary_scratch();
        auto rhs_unit = stack.pop();
        auto lhs_unit = stack.pop();
        auto l_const = StackUtils::maybe_value_u64(lhs_unit);
        auto r_const = StackUtils::maybe_value_u64(rhs_unit);

        if (l_const && r_const) {
            stack.push_const(const_fold(*l_const, *r_const));
            return Generator::compose();
        }

        auto load_lhs = load_numeric_reg_from_pop(lhs_reg, lhs_unit);
        auto load_rhs = load_numeric_reg_from_pop(rhs_reg, rhs_unit);
        bool lhs_ok = StackUtils::is_register<StackUnit, Register>(lhs_unit) || l_const;
        bool rhs_ok = StackUtils::is_register<StackUnit, Register>(rhs_unit) || r_const;
        assert(lhs_ok);
        assert(rhs_ok);

        if (!is_commutative && l_const) {
            stack.unlock_reg(rhs_reg);
            stack.push(RegisterUnit{ lhs_reg });
            return Generator::compose(
                std::move(load_lhs),
                std::move(load_rhs),
                perform_r_r(lhs_reg, rhs_reg)
            );
        }

        if (l_const) {
            stack.unlock_reg(lhs_reg);
            stack.push(RegisterUnit{ rhs_reg });
            return Generator::compose(
                std::move(load_rhs),
                perform_r_imm(rhs_reg, *l_const)
            );
        }

        if (r_const) {
            stack.unlock_reg(rhs_reg);
            stack.push(RegisterUnit{ lhs_reg });
            return Generator::compose(
                std::move(load_lhs),
                perform_r_imm(lhs_reg, *r_const)
            );
        }

        stack.unlock_reg(rhs_reg);
        stack.push(RegisterUnit{ lhs_reg });
        return Generator::compose(
            std::move(load_lhs),
            std::move(load_rhs),
            perform_r_r(lhs_reg, rhs_reg)
        );
    }


public:

    // Prepares a register to be modified in the stack, or suggests a different
    // register by modifying. Returns an optional instruction to inform user
    // if the constant fold was performed or not (with nullopt implying it was)
    std::optional<typename Generator::Instruction> push_reg(Register& desired_reg, auto const_fold) {
        auto top = stack.pop();
        auto top_const = StackUtils::maybe_value_u64(top);

        if (top_const) {
            stack.push_const(const_fold(*top_const));
            return std::nullopt;
        }

        auto emit = load_numeric_reg_from_pop(desired_reg, top);

        bool reg_ok = StackUtils::is_register<StackUnit, Register>(top);
        assert(reg_ok);

        stack.push(RegisterUnit{ desired_reg });
        return emit;
    }

    auto load_numeric_reg_from_pop(Register& reg, StackUnit& unit, bool force_eviction = false) {

        // prevent claiming and therefore locking a register that won't even
        // be used and just early return the registerunit
        // force evicition is necessary for things like the x86 platform (eax and edx)
        // requiring to exist
        auto free = force_eviction ? evict_space_for(reg) : find_free_reg(reg);
        auto norm = normalize_into(reg, unit);
        return Generator::compose(
            std::move(free),
            std::move(norm)
        );
    }

    auto commutative_binary(auto const_fold, auto perform_r_r, auto perform_r_imm) {
        return perform_binary_op(
            const_fold,
            perform_r_r,
            perform_r_imm,
            true
        );
    }

    auto non_commutative_binary(auto const_fold, auto perform_r_r, auto perform_r_imm) {
        return perform_binary_op(
            const_fold,
            perform_r_r,
            perform_r_imm,
            false
        );
    }

    auto assign() {}
};