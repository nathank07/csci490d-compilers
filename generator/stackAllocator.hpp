#pragma once
#include "../analyzer/analyzer.hpp"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <map>
#include <set>
#include <vector>
#include <unordered_map>
#include <variant>


template <typename Derived>
struct TypeSize {
    static uint64_t size_of(const TypedVar& v) {
        const auto visitor = overloads {
            [&](const Int4&) { return Derived::int4_size; },
            [&](const Function&) { return uint64_t{0}; }
        };

        return std::visit(visitor, v);
    }

};


template <typename Register>
struct RegisterUnit { Register in_register; };

struct VirtualRegisterUnit { std::size_t sp_idx; };

// These are the same as a RealStackUnit - but they're known at compile time, allowing
// for constant folding optimizations
struct ValueUnit { uint64_t literal; };

// Used for string pooling or anything else known at compile time
struct StaticPointerUnit { std::size_t s; };

// You get this when you evaluate a node and it's an identifier. It could be
// an symboled expression, in which case you can call .get() on the symbol table, 
// or it could be a keyword.
struct IdentifierUnit { std::string ident; };

namespace StackUtils {

    template <typename StackUnit>
    static std::optional<uint64_t> maybe_value_u64(const StackUnit& unit) {
        if (!std::holds_alternative<ValueUnit>(unit)) return std::nullopt;
        return std::get<ValueUnit>(unit).literal;
    }

    template <typename StackUnit>
    static bool is_virtual(const StackUnit& unit) {
        return std::holds_alternative<VirtualRegisterUnit>(unit);
    }

    template <typename StackUnit, typename Register>
    static bool is_register(const StackUnit& unit) {
        return std::holds_alternative<RegisterUnit<Register>>(unit);
    }

    template <typename StackUnit, typename Register>
    static bool is_register(const StackUnit& unit, Register reg) {
        return std::holds_alternative<RegisterUnit<Register>>(unit)
            && std::get<RegisterUnit<Register>>(unit).in_register == reg;
    }

    template <typename StackUnit>
    static std::string assert_ident(const StackUnit& unit) {
        assert(std::holds_alternative<IdentifierUnit>(unit));
        return std::get<IdentifierUnit>(unit).ident;
    }

}

template <typename Generator, typename Register>
struct StackAllocator {

    using StackUnit = std::variant<
        VirtualRegisterUnit,
        RegisterUnit<Register>,
        ValueUnit, 
        StaticPointerUnit, 
        IdentifierUnit
    >;

    using RegisterTUnit = std::variant<RegisterUnit<Register>, VirtualRegisterUnit>;


private:

    std::vector<StackUnit> eval_stack;

    std::map<std::string, uint64_t> symbol_table;
    std::map<Register, bool> scratch_regs_in_use;
    std::vector<bool> virtual_regs_in_use;    
    std::size_t allocated_symbols = 0;

    void initalize_registers() {}

    template <typename RegisterT, typename... Args>
    void initalize_registers(RegisterT reg, Args... rest) {
        scratch_regs_in_use[reg] = false;
        initalize_registers(rest...);
    }

    std::size_t get_first_free_virtual_reg() {
        auto it = std::find_if(virtual_regs_in_use.begin(), virtual_regs_in_use.end(), [](auto reg) {
            return !reg;
        });

        if (it != virtual_regs_in_use.end()) {
            *it = true;
            return std::distance(virtual_regs_in_use.begin(), it);
        }

        virtual_regs_in_use.push_back(true);
        return virtual_regs_in_use.size() - 1;
    }

public:

    StackAllocator(const std::unordered_map<std::string, TypedVar>& analyzed_symbol_table) {

        for (auto& symbol : analyzed_symbol_table) {
            symbol_table[symbol.first] = allocated_symbols++;
        }

    }

    template <typename... Args>
    StackAllocator(const std::unordered_map<std::string, TypedVar>& analyzed_symbol_table, Args... registers)
        : StackAllocator(analyzed_symbol_table) {
        initalize_registers(registers...);
    }

    uint64_t size() {
        return (allocated_symbols + virtual_regs_in_use.size()) * Generator::stack_size;
    }

    int32_t get(std::string_view symbol) {
        assert(symbol_table.contains(std::string(symbol)));
        return static_cast<int32_t>(symbol_table[std::string(symbol)] * Generator::stack_size);
    }

    int32_t get_vreg(std::size_t sp_idx) {
        return static_cast<int32_t>((allocated_symbols + sp_idx) * Generator::stack_size);
    }

    // Allows for the consumer to put a specific value into desired_reg; 
    // Returns an optional that suggests a different register or makes the
    // caller save the previous register to a specific point in memory
    std::optional<RegisterTUnit> force_space_for(Register desired_reg) {
        auto it = std::find_if(eval_stack.begin(), eval_stack.end(), [&](const StackUnit& u) {
            return StackUtils::is_register<StackUnit, Register>(u, desired_reg);
        });

        if (it == eval_stack.end()) return std::nullopt;

        auto free_scratch = std::find_if(scratch_regs_in_use.begin(), scratch_regs_in_use.end(),
            [](auto& p) { return !p.second; });

        if (free_scratch != scratch_regs_in_use.end()) {
            return RegisterUnit<Register>{ free_scratch->first };
        }

        auto idx = get_first_free_virtual_reg();
        *it = VirtualRegisterUnit{ idx };
        scratch_regs_in_use[desired_reg] = false;
        return RegisterTUnit{ VirtualRegisterUnit{ idx } };
    }

    void push_identifier(const std::string& identifier) {
        eval_stack.push_back(IdentifierUnit{identifier});
    }

    RegisterTUnit push() {
        auto it = std::find_if(scratch_regs_in_use.begin(), scratch_regs_in_use.end(), [](auto& reg){
            return !reg.second;
        });

        if (it != scratch_regs_in_use.end()) {
            it->second = true;
            eval_stack.push_back(RegisterUnit{ it->first });
            return RegisterUnit { it->first };
        }

        auto vr = VirtualRegisterUnit { get_first_free_virtual_reg() };
        eval_stack.push_back(vr);

        return vr;
    }

    void push(StackUnit unit) {
        if (auto* r = std::get_if<RegisterUnit<Register>>(&unit)) {
            scratch_regs_in_use[r->in_register] = true;
        }

        if (auto* r = std::get_if<VirtualRegisterUnit>(&unit)) {
            virtual_regs_in_use[r->sp_idx] = true;
        }

        eval_stack.push_back(std::move(unit));
    }

    void push_const(uint64_t value) {
        push(ValueUnit{ value });
    }

    StackUnit pop() {
        auto top = eval_stack.back();
        eval_stack.pop_back();
        if (auto* r = std::get_if<RegisterUnit<Register>>(&top)) {
            scratch_regs_in_use[r->in_register] = false;
        }

        if (auto* r = std::get_if<VirtualRegisterUnit>(&top)) {
            virtual_regs_in_use[r->sp_idx] = false;
        }

        return top;
    }

    const std::string pop_ident() {
        auto top = eval_stack.back();
        assert(std::holds_alternative<IdentifierUnit>(top));
        eval_stack.pop_back();
        return std::get<IdentifierUnit>(top).ident;
    }

};

