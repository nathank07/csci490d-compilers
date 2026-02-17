#pragma once

#include <cstdint>
#include <variant>
#include <stdint.h>

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

struct Address {

};

struct Immediate {
    int64_t value;
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


using RegisterMemory = std::variant<Register, Address>;

// uint64_t get_byte_64(Register r) {

// }

// uint64_t get_byte_64(Address a) {

// }

// uint64_t get_byte_64(RegisterMemory rm) {

// }

// uint32_t get_byte_32(Register r) {

// }

// uint32_t get_byte_32(Address a) {

// }

inline char get_byte_32(Register r, OpcodeExtension ext) {
    return 0xC0 + (static_cast<int>(r) + static_cast<int>(ext) * 8);
}

inline char get_byte_32(Register dst_r, Register src_r) {
    return get_byte_32(dst_r, static_cast<OpcodeExtension>(src_r));
}



inline char get_byte_32(Address a) {

}


