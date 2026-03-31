#include "../analyzer/analyzer.hpp"
#include <unordered_map>


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

template <typename Generator, typename OffsetT>
struct StackAllocator {

private:

    std::unordered_map<std::string, OffsetT> symbol_table;
    uint64_t curr_sp = 0;

public:

    StackAllocator(const std::unordered_map<std::string, TypedVar>& analyzed_symbol_table) {

        for (auto& symbol : analyzed_symbol_table) {
            curr_sp += Generator::size_of(symbol.second);
            symbol_table[symbol.first] = -static_cast<OffsetT>(curr_sp);
        }

    }

    uint64_t size() {
        return curr_sp;
    }

    OffsetT get(std::string_view symbol) {
        assert(symbol_table.contains(std::string(symbol)));
        return symbol_table[std::string(symbol)];
    }

};