#include "../input.hpp"
#include <fstream>
#include <iostream>

int main() {
    std::ofstream output_file("tests/copy");
    Input input = Input("tests/ChessBoard.tsx");

    int c;

    while ((c = input.consume()) != Input::ErrorCode::OUT_OF_BOUNDS) {
        char ch = static_cast<char> (c);
        output_file << ch;
    }

}