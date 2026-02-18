#pragma once

#include <cstdint>
#include <variant>
#include <stdint.h>
#include "../utils.hpp"

enum class Register {
    EAX = 0,
    ECX,
    EDX,
    EBX,
    ESP,
    EBP,
    ESI,
    EDI
};

enum class ExtendedRegister {
    R8 = 8,
    R9,
    R10,
    R11,
    R12,
    R13,
    R14,
    R15
};

using AnyRegister = std::variant<Register, ExtendedRegister>;

struct ModRM {
    AnyRegister reg;
};

struct Immediate {
    uint32_t value;
};

enum class OpcodeExtension {
    Zero = 0,
    One,
    Two,
    Three,
    Four,
    Five,
    Six,
    Seven,
};

inline uint8_t get_byte_32(Register r, OpcodeExtension ext) {
    return static_cast<uint8_t>(0xC0 | (static_cast<uint8_t>(r) | static_cast<uint8_t>(ext) << 3));
}

inline uint8_t get_byte_32_r_rm(Register dst_r, Register src_r) {
    return get_byte_32(dst_r, static_cast<OpcodeExtension>(src_r));
}

inline uint8_t get_byte_32_rm_r(Register dst_r, Register src_r) {
    return get_byte_32(src_r, static_cast<OpcodeExtension>(dst_r));
}

inline Register get_lower_order(ExtendedRegister r) {
    return static_cast<Register>(static_cast<uint8_t>(r) & 0x7);
}

inline Register get_lower_order(AnyRegister r) {
    
    auto visitor = overloads {
        [&](Register reg) { return reg; },
        [&](ExtendedRegister reg) { return get_lower_order(reg); },
    };

    return std::visit(visitor, r);
}

inline uint8_t rex_bit(AnyRegister r) {
    return std::holds_alternative<ExtendedRegister>(r);
}

inline uint8_t rex_bit(ModRM r) {
    return std::holds_alternative<ExtendedRegister>(r.reg);
}