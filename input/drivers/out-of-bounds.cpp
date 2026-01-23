#include "../input.hpp"
#include <iostream>

int main() {
    Input input = Input("tests/aoc2024day1.hs");

    for (int i = 0; i < 1000; i++) {
        input.next();
    }

    std::cout << input.peek() << "\n";

    input.back();

    std::cout << input.peek() << "\n";

    /*  
        What is the behavior of the get character functions? What should it be? 

        peek() as it's currently implemented returns -2 (Input::ErrorCode::OUT_OF_BOUNDS).
        If using consume() or next(), it won't move the index anymore, but will still
        be considered out of bounds.

        What is the behavior of the unget function? What should it be?

        Since the index is at the end of the file + 1, it will return
        the last character in the file. When buff_idx is 0, it won't
        move any further backwards, and it will stick at 0.
    
    */
}