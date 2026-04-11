#pragma once
#include "../stackOperator.hpp"
#include "x86Instructions.hpp"

template<typename Generator>
struct x86StackOperator : StackOperator<Generator> {

    using StackOperator<Generator>::stack;

    enum class DivType {
        MOD,
        DIV
    };

    auto handle_div_family(DivType type) {
        auto rhs_unit = stack.pop();
        auto lhs_unit = stack.pop();
        assert(!StackUtils::maybe_static_ptr(rhs_unit));
        assert(!StackUtils::maybe_static_ptr(lhs_unit));
        auto l_const = StackUtils::maybe_value_u64(lhs_unit);
        auto r_const = StackUtils::maybe_value_u64(rhs_unit);

        if (l_const && r_const) {
            stack.push_const(
                type == DivType::DIV ?
                *l_const / *r_const :
                *l_const % *r_const
            );
            return x86::compose();
        }

        auto lhs_reg = x86::Register::EAX;
        auto rhs_reg = Generator::primary_scratch();

        auto load_lhs = this->load_reg_from_pop(lhs_reg, lhs_unit, true);
        auto load_rhs = this->load_reg_from_pop(rhs_reg, rhs_unit);

        auto mov_eax = type == DivType::MOD ? 
            x86::mov(lhs_reg, x86::Register::EDX) : 
            x86::compose();

        stack.unlock_reg(rhs_reg);
        stack.push(RegisterUnit { lhs_reg });
        
        return x86::compose(
            std::move(load_lhs),
            std::move(load_rhs),
            x86::cdq(),
            x86::idiv(rhs_reg),
            std::move(mov_eax)
        );
        
    }
};
