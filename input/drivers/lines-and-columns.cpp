#include "../input.hpp"
#include <iostream>

int main() {
    Input input = Input("tests/aoc2024day1.hs");

    int c;

    while ((c = input.peek()) != Input::ErrorCode::OUT_OF_BOUNDS) {
        char ch = static_cast<char> (c);
        auto line = input.get_line_number();
        auto col = input.get_col_number();
        
        std::cout << "Line " << line << ", Col " << col << ": " << ch << std::endl;
        input.next();
    }

    auto line = input.get_line_number();

    for (auto i = 1; i <= line; i++) {
        std::cout << "Line " << i << ": " << input.get_line(i);
    }
}