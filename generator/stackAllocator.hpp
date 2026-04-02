#include "../analyzer/analyzer.hpp"
#include <cstdint>
#include <cstring>
#include <stack>
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

// Real values that fit into one stack unit - an example of this could be an int,
// if an int size is 8 and the stack unit size is 8, you'd get this specific object.
struct RealUnit {};

// These are the same as a RealUnit - but they're known at compile time, allowing
// for constant folding optimizations
struct ValueUnit {
    uint64_t literal;
};

// A real value, but doesn't fit into a single stack unit. It contains the how
// many stack units it has (size), and a pointer to the address of memory 
// at the current point of time. Note that you are expected to consume the pointer
// prior to pushing to the stack because otherwise you'll overwrite it.
struct PointerUnit {
    uint8_t size;
    int64_t bp_offset;
};

// You get this when you evaluate a node and it's an identifier. It could be
// an symboled expression, in which case you can call .get() on the symbol table, 
// or it could be a keyword.
struct IdentifierUnit {
    std::string literal;
};

template <typename Generator>
struct StackAllocator {

    using StackUnit = std::variant<RealUnit, ValueUnit, PointerUnit, IdentifierUnit>;

private:

    std::stack<StackUnit> eval_stack;
    uint64_t eval_stack_size = 0;

    std::unordered_map<std::string, uint64_t> symbol_table;
    uint64_t allocated_symbol_size = 0;
    
    template <typename T>
    typename Generator::Instruction push_large_value(T) {
        //todo make generic version of this
        assert(false && "unimplemented stackallocator func");
        return Generator::compose();
    }

    auto push_large_value(std::u8string value) {

        auto init = Generator::compose();

        value += '\0';
        while(value.size() % sizeof(uint64_t) != 0) value += '\0';

        for (std::size_t i = 0; i < value.size(); i += sizeof(uint64_t)) {
            uint64_t val = 0;
            std::memcpy(&val, value.data() + i, sizeof(uint64_t));
            init = Generator::compose(
                Generator::push_imm(val),
                std::move(init)
            );
        }

        uint64_t string_bytes = value.size();
        eval_stack.push(PointerUnit{
            static_cast<uint8_t>(string_bytes / sizeof(uint64_t)),
            -static_cast<int64_t>(allocated_symbol_size + eval_stack_size + string_bytes)
        });
        eval_stack_size += string_bytes;

        return init;
    }

public:

    StackAllocator(const std::unordered_map<std::string, TypedVar>& analyzed_symbol_table) {

        for (auto& symbol : analyzed_symbol_table) {
            allocated_symbol_size += Generator::size_of(symbol.second);
            symbol_table[symbol.first] = allocated_symbol_size;
        }

    }

    uint64_t symbols_size() {
        return allocated_symbol_size;
    }

    int32_t get(std::string_view symbol) {
        assert(symbol_table.contains(std::string(symbol)));
        return static_cast<int32_t>(-symbol_table[std::string(symbol)]);
    }

    void push_identifier(const std::string& identifier) {
        eval_stack.push(IdentifierUnit{identifier});
    }

    void push_real_value() {
        eval_stack.push(RealUnit{});
        eval_stack_size += sizeof(uint64_t);
    }

    template <typename T>
    auto push_value(T value) {
        if constexpr (std::is_same_v<T, std::u8string>) {
            return push_large_value(std::move(value));
        } else if (sizeof(value) <= sizeof(uint64_t)) {
            eval_stack.push(ValueUnit{ value });
            return Generator::compose();
        } 

        return push_large_value(value);
    }

    StackUnit pop() {
        auto top = eval_stack.top();
        eval_stack.pop();
        eval_stack_size -= std::visit(overloads {
            [](ValueUnit&)       { return uint64_t{0}; },
            [](RealUnit&)        { return sizeof(uint64_t); },
            [](PointerUnit& p)   { return p.size * sizeof(uint64_t); },
            [](IdentifierUnit&)  { return uint64_t{0}; }
        }, top);
        return top;
    }

    static std::optional<uint64_t> maybe_value_u64(const StackUnit& unit) {
        if (!std::holds_alternative<ValueUnit>(unit)) return std::nullopt;
        return std::get<ValueUnit>(unit).literal;
    }

    static bool is_real(const StackUnit& unit) {
        return std::holds_alternative<RealUnit>(unit);
    }

    static std::string assert_ident(const StackUnit& unit) {
        assert(std::holds_alternative<IdentifierUnit>(unit));
        return std::get<IdentifierUnit>(unit).literal;
    }

};