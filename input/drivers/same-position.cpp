#include "../input.hpp"
#include <fstream>
#include <iostream>

int main() {
    Input input = Input::create("tests/ChessBoard.tsx").value();
    std::ofstream output1("tests/init");
    std::ofstream output2("tests/final");

    for (int i = 0; i < 300; i++) {
        output1 << "Line: "  << input.get_line_number() 
                << ", Col: " << input.get_col_number() << std::endl;
        input.next();
    }

    for (int i = 0; i < 300; i++) {
        input.back();
        output2 << "Line: "  << input.get_line_number() 
                << ", Col: " << input.get_col_number() << std::endl;
    }

    
}