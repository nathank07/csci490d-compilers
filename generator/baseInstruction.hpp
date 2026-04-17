#pragma once
#include <cassert>
#include <ostream>
#include <string>
#include "../lexer/token.hpp"

struct BaseInstruction {
    std::string emitted_content;
    size_t byte_size = 0;

    void operator()(std::ostream& os) const { os << emitted_content; }

    friend std::ostream& operator<<(std::ostream& os, const BaseInstruction& b) {
        os << b.emitted_content;
        return os;
    }
};

template <typename Derived>
struct InstructionControl {

    enum class Conditional {
        EQ, NEQ, 
        LT, LTE, 
        GT, GTE,
        UNCONDITIONALLY,
        NEVER
    };

    static constexpr Conditional invert(Conditional cond) {
        switch (cond) {
            case Conditional::LT:  return Conditional::GTE;
            case Conditional::GT:  return Conditional::LTE;
            case Conditional::EQ:  return Conditional::NEQ;
            case Conditional::LTE: return Conditional::GT;
            case Conditional::GTE: return Conditional::LT;
            case Conditional::NEQ: return Conditional::EQ;
            case Conditional::UNCONDITIONALLY: return Conditional::NEVER;
            case Conditional::NEVER: return Conditional::UNCONDITIONALLY;
        }
        __builtin_unreachable();
    }

    static constexpr Conditional tok_cond(const TokenType& t) {
        switch (t) {
            case TokenType::LESS_THAN: return Conditional::LT;
            case TokenType::LESS_THAN_EQ: return Conditional::LTE;
            case TokenType::GREATER_THAN: return Conditional::GT;
            case TokenType::GREATER_THAN_EQ: return Conditional::GTE;
            case TokenType::EQUALS: return Conditional::EQ;
            case TokenType::NOT_EQUALS: return Conditional::NEQ;
            default: { 
                assert(false && "Non conditional used as conditional");
                return Conditional::NEVER;
            }
        }
    }

    // These assume that the IP jumps from the beginning of the instruction:
    // eg. JMP 0 is a no op.
    static auto skip_block(auto block, Conditional cond = Conditional::UNCONDITIONALLY) {
        switch (cond) {
            case Conditional::LT:  return Derived::skip_block_lt(block);
            case Conditional::GT:  return Derived::skip_block_gt(block);
            case Conditional::EQ:  return Derived::skip_block_eq(block);
            case Conditional::LTE: return Derived::skip_block_lte(block);
            case Conditional::GTE: return Derived::skip_block_gte(block);
            case Conditional::NEQ: return Derived::skip_block_neq(block);
            case Conditional::UNCONDITIONALLY: return Derived::skip_block_un(block);
            case Conditional::NEVER: return Derived::compose();
        }
        __builtin_unreachable();
    }

    static auto _while(auto check_cond, Conditional cond, auto do_this) {
        return _if(check_cond, cond, Derived::compose(
            do_this, 
            Derived::jump_rel(-_if(check_cond, cond, do_this).byte_size)
        ));
    }

    static auto _while(auto v1, auto v2, auto cond, auto do_this) {
        auto check_cond = Derived::compare(v1, v2);
        return _while(check_cond, cond, do_this);
    }

    static auto do_while(auto check_cond, Conditional cond, auto do_this) {
        return Derived::compose(
            do_this,
            _if(check_cond, cond, Derived::jump_rel(-do_this.byte_size))
        );
    }

    static auto do_while(auto v1, auto v2, auto cond, auto do_this) {
        return do_while(Derived::compare(v1, v2), cond, do_this);
    }

    static auto skip_if(auto check_cond, auto cond, auto do_this) {
        return Derived::compose(
            check_cond,
            skip_block(do_this, cond)
        );
    }

    static auto skip_if(auto v1, auto v2, auto cond, auto do_this) {
        return skip_if(Derived::compare(v1, v2), cond, do_this);
    }

    static auto _if(auto check_cond, auto cond, auto do_this) {
        return skip_if(check_cond, invert(cond), do_this);
    }

    static auto _if_else(auto check_cond, auto cond, auto do_this, auto or_this) {
        return Derived::compose(
            _if(check_cond, cond, Derived::compose(
                do_this,
                Derived::jump_rel(or_this.byte_size)
            )),
            or_this
        );        
    }

};
