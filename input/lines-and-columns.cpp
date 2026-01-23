#include "input.hpp"
#include <iostream>

int main() {
    Input i = Input("test.txt");

    int c = i.peek();

    while (i.peek() != Input::ErrorCode::OUT_OF_BOUNDARY) {
    //for(int j = 0; j < 100; j++) {
        c = i.peek();
        i.next();

        char ch = static_cast<char> (c);
        auto line = i.get_line_number();
        auto col = i.get_col_number();
        
        std::cout << "Line " << line << ", Col " << col << ": " << ch << std::endl;
    }

    auto line = i.get_line_number();

    for (auto j = 1; j <= line; j++) {
        std::cout << "Line " << j << ": " << i.get_line(j) << "\n";
    }
}