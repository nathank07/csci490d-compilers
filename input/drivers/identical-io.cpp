#include "../input.hpp"
#include <fstream>
#include <iostream>

int main() {
    std::ofstream output_file("tests/copy");
    auto input = Input::create("tests/ChessBoard.tsx").value();
    
    while (auto ch = input.consume()) {
        output_file << *ch;
    }
}