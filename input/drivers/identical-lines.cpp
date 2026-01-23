#include "../input.hpp"
#include <iostream>

int main() {
    Input input = Input("tests/aoc2024day1.hs");

    while (input.consume() != Input::ErrorCode::OUT_OF_BOUNDS) {}

    for (auto i = 1; i < input.get_line_number(); i++) {
        std::cout << input.get_line(i);
    }

}