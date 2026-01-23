#include "../input.hpp"
#include <fstream>
#include <iostream>

int main() {
    Input input = Input::create("tests/ChessBoard.tsx").value();

    std::cout << "Line: "  << input.get_line_number() 
              << ", Col: " << input.get_col_number() << std::endl;

    for (int i = 0; i < 300; i++) {
        input.next();
    }

    for (int i = 0; i < 300; i++) {
        input.back();
    }

    std::cout << "Line: "  << input.get_line_number() 
              << ", Col: " << input.get_col_number() << std::endl;
}