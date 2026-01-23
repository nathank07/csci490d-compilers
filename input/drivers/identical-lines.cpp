#include "../input.hpp"
#include <iostream>

int main() {
    auto input = Input::create("tests/aoc2024day1.hs").value();

    while (input.consume()) {}

    for (auto i = 1; i <= input.get_line_number(); i++) {
        auto line = input.get_line(i);
        if(line) std::cout << "Line " << i << ": " << *line;
    }

}
