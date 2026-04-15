#pragma once
#include "stackAllocator.hpp"
#include <cassert>
#include <cstdlib>
#include <functional>
#include <iostream>

template <typename Generator>
struct BaseBinary {
    using Register    = typename Generator::Register;
    using Instruction = typename Generator::Instruction;

    template <typename S, typename D>
    using F = std::optional<std::function<Instruction(S, D)>>;

    std::function<Instruction(Register, Register)> 
        handle_dreg_sreg;

    std::optional<std::function<uint64_t(uint64_t, uint64_t)>> 
        handle_const_fold;

    F<Register, uint64_t> handle_dreg_imm;
    F<Register, int32_t>  handle_dreg_smem;

    // These are fairly niche; they only really get triggered if
    // the registers spill into vregs; so anything above this should
    // be prioritized much more heavily
    F<int32_t, uint64_t>  handle_dmem_imm;
    F<int32_t, Register>  handle_dmem_sreg;
    F<int32_t, int32_t>   handle_dmem_smem;
    bool is_commutative = false;
};


template <typename Generator>
struct StackOperator {

    StackAllocator<Generator, typename Generator::Register>& stack;

    using Register = typename Generator::Register;
    using StackUnit = typename StackAllocator<Generator, typename Generator::Register>::StackUnit;



    /* 
        Evicts your desired reg and returns an instruction on what you need to
        do to save the register you evicted.
    */
    auto evict_space_for(Register desired_reg) {
        auto dest = stack.lock_reg(desired_reg);
        if (!dest) return Generator::compose();
        return std::visit(overloads {
            [&](RegisterUnit<Register>& u) {
                return Generator::mov_dreg_sreg(u.in_register, desired_reg);
            },
            [&](VirtualRegisterUnit& u) {
                return Generator::mov_doffset_sreg(stack.get_vreg(u.sp_idx), desired_reg);
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
                    return Generator::mov_dreg_sreg(u.in_register, desired_reg);
                }
                desired_reg = u.in_register;
                return Generator::compose();
            },
            [&](VirtualRegisterUnit& u) {
                return Generator::mov_doffset_sreg(stack.get_vreg(u.sp_idx), desired_reg);
            }
        }, *dest);
    }
    
private:

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
                return Generator::mov_dreg_soffset(reg, offset);
            },
            [&](VirtualRegisterUnit& u) {
                auto offset = stack.get_vreg(u.sp_idx);
                unit = RegisterUnit<Register>{ reg };
                return Generator::mov_dreg_soffset(reg, offset);
            },
            [&](RegisterUnit<Register>& u) {
                if (u.in_register == reg) return Generator::compose();
                auto emit = Generator::mov_dreg_sreg(reg, u.in_register);
                stack.unlock_reg(u.in_register);
                unit = RegisterUnit<Register>{ reg };
                return emit;
            },
            [&](ValueUnit& u) {
                auto emit = Generator::mov_dreg_imm(reg, u.literal);
                unit = RegisterUnit<Register>{ reg };
                return emit;
            },
            [&](StaticPointerUnit& u) {
                auto emit = Generator::mov_dreg_sstatic_ptr(reg, u.abs_addr);
                unit = RegisterUnit<Register>{ reg };
                return emit;
            }
        }, unit);
    }

public:

    auto perform_binary_op(BaseBinary<Generator> b) {

        auto rhs_reg = Generator::primary_scratch();
        auto lhs_reg = Generator::secondary_scratch();
        auto rhs_unit = stack.pop();
        auto lhs_unit = stack.pop();


        if (auto* r = std::get_if<RegisterUnit<Register>>(&rhs_unit)) {
            rhs_reg = r->in_register;
        }

        if (auto* r = std::get_if<RegisterUnit<Register>>(&lhs_unit)) {
            if (r->in_register != rhs_reg) lhs_reg = r->in_register;
        }


        assert(!StackUtils::maybe_static_ptr(rhs_unit));
        assert(!StackUtils::maybe_static_ptr(lhs_unit));

        auto l_const = StackUtils::maybe_value_u64(lhs_unit);
        auto r_const = StackUtils::maybe_value_u64(rhs_unit);

        if (l_const && r_const && b.handle_const_fold) {
            stack.push_const((*b.handle_const_fold)(*l_const, *r_const));
            return Generator::compose();
        }


        // load_reg_from_pop mutates; so store the original stackunit
        // in the event we want to preform a computation that does not
        // use one of these normalized registers (for instance reg <- vreg)
        auto s_lhs = lhs_unit;
        auto s_rhs = rhs_unit;

        auto load_lhs = load_reg_from_pop(lhs_reg, lhs_unit);
        auto load_rhs = load_reg_from_pop(rhs_reg, rhs_unit);
        bool lhs_ok = StackUtils::is_register<StackUnit, Register>(lhs_unit) || l_const;
        bool rhs_ok = StackUtils::is_register<StackUnit, Register>(rhs_unit) || r_const;
        assert(lhs_ok);
        assert(rhs_ok);
        
        auto l_is_reg = StackUtils::is_register<StackUnit, Register>(s_lhs);
        auto r_is_reg = StackUtils::is_register<StackUnit, Register>(s_rhs);
        auto l_vreg = StackUtils::maybe_virtual(s_lhs);
        auto r_vreg = StackUtils::maybe_virtual(s_rhs);
        auto l_ident = StackUtils::maybe_ident(s_lhs);
        auto r_ident = StackUtils::maybe_ident(s_rhs);

        if (l_is_reg) {

            if (r_const && b.handle_dreg_imm) {
                stack.unlock_reg(rhs_reg); stack.push(RegisterUnit{ lhs_reg });
                return Generator::compose(
                    std::move(load_lhs),
                    (*b.handle_dreg_imm)(lhs_reg, *r_const)
                );
            }

            if (r_is_reg) {
                stack.unlock_reg(rhs_reg); stack.push(RegisterUnit{ lhs_reg });
                return Generator::compose(
                    std::move(load_lhs),
                    std::move(load_rhs),
                    b.handle_dreg_sreg(lhs_reg, rhs_reg)
                );
            }

            if (r_vreg && b.handle_dreg_smem) {
                stack.unlock_reg(rhs_reg); stack.push(RegisterUnit{ lhs_reg });
                return Generator::compose(
                    std::move(load_lhs),
                    (*b.handle_dreg_smem)(lhs_reg, stack.get_vreg(*r_vreg))
                );
            }

            if (r_ident && b.handle_dreg_smem) {
                stack.unlock_reg(rhs_reg); stack.push(RegisterUnit{ lhs_reg });
                return Generator::compose(
                    std::move(load_lhs),
                    (*b.handle_dreg_smem)(lhs_reg, stack.get(*r_ident))
                );
            }

        }
        
        if (r_is_reg && b.is_commutative) {

            if (l_const && b.handle_dreg_imm) {
                stack.unlock_reg(lhs_reg); stack.push(RegisterUnit{ rhs_reg });
                return Generator::compose(
                    std::move(load_rhs),
                    (*b.handle_dreg_imm)(rhs_reg, *l_const)
                );
            }

            if (l_vreg && b.handle_dreg_smem) {
                stack.unlock_reg(lhs_reg); stack.push(RegisterUnit{ rhs_reg });
                return Generator::compose(
                    std::move(load_rhs),
                    (*b.handle_dreg_smem)(rhs_reg, stack.get_vreg(*l_vreg))
                );
            }

            if (l_ident && b.handle_dreg_smem) {
                stack.unlock_reg(lhs_reg); stack.push(RegisterUnit{ rhs_reg });
                return Generator::compose(
                    std::move(load_rhs),
                    (*b.handle_dreg_smem)(rhs_reg, stack.get(*l_ident))
                );
            }
            
        }

        if (r_is_reg && !b.is_commutative) {

            if (l_vreg && b.handle_dmem_sreg) {
                stack.unlock_reg(lhs_reg); stack.unlock_reg(rhs_reg);
                stack.push(VirtualRegisterUnit { *l_vreg });
                return Generator::compose(
                    std::move(load_rhs),
                    (*b.handle_dmem_sreg)(stack.get_vreg(*l_vreg), rhs_reg)
                );
            }

            // <l_ident intentionally left out (fallthrough normalization)>

            // <l_const intentionally left out (fallthrough normalization)>

        }

        if (l_vreg) {

            if (r_const && b.handle_dmem_imm) {
                stack.unlock_reg(lhs_reg); stack.unlock_reg(rhs_reg);
                stack.push(VirtualRegisterUnit { *l_vreg });
                return (*b.handle_dmem_imm)(stack.get_vreg(*l_vreg), *r_const);
            }

            // <l_vreg, r_reg handled previously>

            if (r_vreg && b.handle_dmem_smem) {
                stack.unlock_reg(lhs_reg); stack.unlock_reg(rhs_reg);
                stack.push(VirtualRegisterUnit { *l_vreg });
                return (*b.handle_dmem_smem)(stack.get_vreg(*l_vreg), stack.get_vreg(*r_vreg));
            }

            if (r_ident && b.handle_dmem_smem) {
                stack.unlock_reg(lhs_reg); stack.unlock_reg(rhs_reg);
                stack.push(VirtualRegisterUnit { *l_vreg });
                return (*b.handle_dmem_smem)(stack.get_vreg(*l_vreg), stack.get(*r_ident));
            }

        }

        if (r_vreg && b.is_commutative) {

            if (l_const && b.handle_dmem_imm) {
                stack.unlock_reg(lhs_reg); stack.unlock_reg(rhs_reg);
                stack.push(VirtualRegisterUnit { *r_vreg });
                return (*b.handle_dmem_imm)(stack.get_vreg(*r_vreg), *l_const);
            }

            // <l_reg, r_vreg previously handled>
            // <l_vreg, r_vreg previously handled>

            if (l_ident && b.handle_dmem_smem) {
                stack.unlock_reg(lhs_reg); stack.unlock_reg(rhs_reg);
                stack.push(VirtualRegisterUnit { *r_vreg });
                return (*b.handle_dmem_smem)(stack.get_vreg(*r_vreg), stack.get(*l_ident));
            }

        }

        stack.unlock_reg(rhs_reg);
        stack.push(RegisterUnit{ lhs_reg });
        return Generator::compose(
            std::move(load_lhs),
            std::move(load_rhs),
            b.handle_dreg_sreg(lhs_reg, rhs_reg)
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

        auto emit = load_reg_from_pop(desired_reg, top);

        bool reg_ok = StackUtils::is_register<StackUnit, Register>(top);
        assert(reg_ok);

        stack.push(RegisterUnit{ desired_reg });
        return emit;
    }

    auto load_reg_from_pop(Register& reg, StackUnit& unit, bool force_eviction = false) {

        if (!force_eviction) {
            if (auto* r = std::get_if<RegisterUnit<Register>>(&unit)) {
                if (r->in_register == reg) return Generator::compose();
            }
        }

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

    auto assign() {}
};