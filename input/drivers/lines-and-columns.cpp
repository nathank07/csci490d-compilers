#include "../input.hpp"
#include <iostream>

int main() {
    auto input = Input::create("tests/aoc2024day1.hs").value();

    auto c = input.peek();

    while (c = input.peek()) {
        auto line = input.get_line_number();
        auto col = input.get_col_number();
        
        std::cout << "Line " << line << ", Col " << col << ": " << *c << std::endl;
        input.next();
    }

}
