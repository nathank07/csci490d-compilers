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


template <typename Generator>
struct RegisterUnit { typename Generator::Register in_register; };

struct VirtualRegisterUnit { std::size_t sp_idx; };

template <typename Generator>
struct LogicalComparisonUnit { typename Generator::Conditional cond; };

// These are the same as a RealStackUnit - but they're known at compile time, allowing
// for constant folding optimizations
struct ValueUnit { uint64_t literal; };

// Used for string pooling or anything else known at compile time
struct StaticPointerUnit { std::size_t abs_addr; };

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
    static std::optional<std::size_t> maybe_static_ptr(const StackUnit& unit) {
        if (!std::holds_alternative<StaticPointerUnit>(unit)) return std::nullopt;
        return std::get<StaticPointerUnit>(unit).abs_addr;
    }

    template <typename StackUnit>
    static std::optional<std::string> maybe_ident(const StackUnit& unit) {
        if (!std::holds_alternative<IdentifierUnit>(unit)) return std::nullopt;
        return std::get<IdentifierUnit>(unit).ident;
    }

    template <typename StackUnit>
    static std::optional<std::size_t> maybe_virtual(const StackUnit& unit) {
        if (!std::holds_alternative<VirtualRegisterUnit>(unit)) return std::nullopt;
        return std::get<VirtualRegisterUnit>(unit).sp_idx;
    }

    template <typename StackUnit>
    static bool is_virtual(const StackUnit& unit) {
        return std::holds_alternative<VirtualRegisterUnit>(unit);
    }

    template <typename StackUnit, typename Generator>
    static bool is_register(const StackUnit& unit) {
        return std::holds_alternative<RegisterUnit<Generator>>(unit);
    }

    template <typename StackUnit, typename Generator>
    static bool is_register(const StackUnit& unit, typename Generator::Register reg) {
        return std::holds_alternative<RegisterUnit<Generator>>(unit)
            && std::get<RegisterUnit<Generator>>(unit).in_register == reg;
    }

    template <typename StackUnit>
    static std::string assert_ident(const StackUnit& unit) {
        assert(std::holds_alternative<IdentifierUnit>(unit));
        return std::get<IdentifierUnit>(unit).ident;
    }

}

template <typename Generator>
struct StackAllocator {

    using Register = typename Generator::Register;

    using StackUnit = std::variant<
        VirtualRegisterUnit,
        RegisterUnit<Generator>,
        // LogicalComparisonUnit<Generator>,
        ValueUnit,
        StaticPointerUnit,
        IdentifierUnit
    >;

    using RegisterTUnit = std::variant<RegisterUnit<Generator>, VirtualRegisterUnit>;


private:

    std::vector<StackUnit> eval_stack;

    std::map<std::string, uint64_t> symbol_table;
    std::set<Register> scratch_regs;
    std::set<Register> locked_regs;
    std::vector<bool> virtual_regs_in_use;
    std::size_t allocated_symbols = 0;

    void initalize_registers() {}

    template <typename RegisterT, typename... Args>
    void initalize_registers(RegisterT reg, Args... rest) {
        scratch_regs.insert(reg);
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
    //
    // If the register returned is specified as scratch register, it will be prevented
    // from being used as scratch until it is either pushed and popped, or, 
    // more importantly, freed via free_space_for(). This means that the register
    // must be accounted for by either pushing or freeing.
    std::optional<RegisterTUnit> lock_reg(Register desired_reg) {
        auto it = std::find_if(eval_stack.begin(), eval_stack.end(), [&](const StackUnit& u) {
            return StackUtils::is_register<StackUnit, Generator>(u, desired_reg);
        });

        if (it == eval_stack.end() && !locked_regs.contains(desired_reg)) {
            locked_regs.insert(desired_reg);
            return std::nullopt;
        }

        std::vector<Register> free_scratches;
        std::set_difference(
            scratch_regs.begin(), scratch_regs.end(),
            locked_regs.begin(), locked_regs.end(),
            std::back_inserter(free_scratches)
        );

        if (!free_scratches.empty()) {
            auto reg = free_scratches.front();
            locked_regs.insert(desired_reg);
            locked_regs.insert(reg);
            *it = RegisterUnit<Generator>{ reg };
            return RegisterUnit<Generator>{ reg };
        }

        locked_regs.insert(desired_reg);
        auto idx = get_first_free_virtual_reg();
        if (it != eval_stack.end()) {
            *it = VirtualRegisterUnit{ idx };
        }
        return RegisterTUnit{ VirtualRegisterUnit{ idx } };
    }

    void unlock_reg(Register locked_reg) {
        locked_regs.erase(locked_reg);
    }

    bool in_stack(Register reg) {
        return std::any_of(eval_stack.begin(), eval_stack.end(), [&](const StackUnit& u) {
            return StackUtils::is_register<StackUnit, Generator>(u, reg);
        });
    }

    void push_identifier(const std::string& identifier) {
        eval_stack.push_back(IdentifierUnit{identifier});
    }

    void push_static_ptr(const uint64_t abs) {
        eval_stack.push_back(StaticPointerUnit{abs});
    }

    RegisterTUnit push() {
        std::vector<Register> free_scratches;
        std::set_difference(
            scratch_regs.begin(), scratch_regs.end(),
            locked_regs.begin(), locked_regs.end(),
            std::back_inserter(free_scratches)
        );

        if (!free_scratches.empty()) {
            auto reg = free_scratches.front();
            locked_regs.insert(reg);
            eval_stack.push_back(RegisterUnit<Generator>{ reg });
            return RegisterUnit<Generator>{ reg };
        }

        auto vr = VirtualRegisterUnit { get_first_free_virtual_reg() };
        eval_stack.push_back(vr);

        return vr;
    }

    void push(StackUnit unit) {
        if (auto* r = std::get_if<RegisterUnit<Generator>>(&unit)) {
            locked_regs.insert(r->in_register);
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

