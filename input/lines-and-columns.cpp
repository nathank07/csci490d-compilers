#include "input.hpp"
#include <iostream>

int main() {
    Input i = Input("test.txt");

    int c = i.peek();

    while (i.peek() != Input::ErrorCode::OUT_OF_BOUNDARY) {
        c = i.peek();
        i.next();

        char ch = static_cast<char> (c);
        auto line = i.get_line();
        auto col = i.get_col();
        
        std::cout << "Line " << line << ", Col " << col << ": " << ch << std::endl;
    }

    std::cout << std::endl;

}