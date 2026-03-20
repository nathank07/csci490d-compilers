#pragma once
#include <ostream>
#include <string>

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

    static auto _while(auto v1, auto v2, auto cond, auto do_this) {

        auto check_cond = Derived::compare(v1, v2);

        int32_t addr = -static_cast<int32_t>
            (do_this.byte_size + check_cond.byte_size);

        return Derived::compose(
            Derived::jump_rel(static_cast<int32_t>(do_this.byte_size)),
            do_this,
            check_cond,
            Derived::jump_rel_when(addr, cond)
        );
    }

    static auto do_while(auto v1, auto v2, auto cond, auto do_this) {

        auto check_cond = Derived::compare(v1, v2);

        int32_t addr = -static_cast<int32_t>(do_this.byte_size + check_cond.byte_size);

        return Derived::compose(
            do_this,
            check_cond,
            Derived::jump_rel_when(addr, cond)
        );
    }

    static auto skip_if(auto v1, auto v2, auto cond, auto do_this) {
        return Derived::compose(
            Derived::compare(v1, v2),
            Derived::jump_rel_when(static_cast<int32_t>(do_this.byte_size), cond),
            do_this
        );
    }

    static auto _if(auto v1, auto v2, auto cond, auto do_this) {

        auto compare = Derived::compare(v1, v2);

        // Create an if statement with no real location yet because
        // the jumps rely on measuring each other
        auto dummy_if_block = Derived::compose(
            do_this,
            Derived::jump_rel(0)
        );

        // Because this algorithm skips the initial block, it needs to compare
        // at the bottom of the block and go back to the top. This allows 
        // architectures with limited conditionals like in midos (GT, LT, EQ)
        // to have if statements instead of skip_if statements 
        auto jump_back = Derived::compose(
            compare,
            Derived::jump_rel_when(-dummy_if_block.byte_size, cond)
        );

        // Fill it in once getting the real size
        auto real_if_block = Derived::compose(
            do_this,
            Derived::jump_rel(jump_back.byte_size)
        );

        // Needed because on some architecture,
        // like x86, jump_rel can vary in size
        if (real_if_block.byte_size != dummy_if_block.byte_size) {
            jump_back = Derived::compose(
                compare,
                Derived::jump_rel_when(-real_if_block.byte_size, cond)
            );
        }
        
        return Derived::compose(
            Derived::jump_rel(real_if_block.byte_size),
            real_if_block,
            jump_back
        );
    }

    static auto _if_else(auto v1, auto v2, auto cond, auto do_this, auto or_this) {

        // See _if() comments for explanation. Functionally the same as _if(),
        // but measures for the else block to skip it if the statement is true.

        auto compare = Derived::compare(v1, v2);

        auto dummy_if_block = Derived::compose(
            do_this,
            Derived::jump_rel(or_this.byte_size)
        );

        auto jump_back = Derived::compose(
            compare,
            Derived::jump_rel_when(-dummy_if_block.byte_size, cond)
        );

        // Aforementioned skipping if true (or_this.byte_size)
        auto real_if_block = Derived::compose(
            do_this,
            Derived::jump_rel(jump_back.byte_size + or_this.byte_size)
        );

        if (real_if_block.byte_size != dummy_if_block.byte_size) {
            jump_back = Derived::compose(
                compare,
                Derived::jump_rel_when(-real_if_block.byte_size, cond)
            );
        }
    
        return Derived::compose(
            Derived::jump_rel(real_if_block.byte_size),
            real_if_block,
            jump_back,
            or_this
        );
    }
};
