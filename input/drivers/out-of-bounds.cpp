#include "../input.hpp"
#include <iostream>

int main() {
    Input input = Input::create("tests/aoc2024day1.hs").value();

    for (int i = 0; i < 1000; i++) {
        input.next();
    }

    input.peek()
        .transform([](auto c) { 
            std::cout << c << "\n"; return c;
        })
        .or_else([] -> std::optional<unsigned char> { 
            std::cout << "EOF" << "\n"; return std::nullopt;
        });
    
    input.back();
    input.back();

    input.peek()
        .transform([](auto c) { 
            std::cout << c << "\n"; return c;  
        })
        .or_else([] -> std::optional<unsigned char> { 
            std::cout << "EOF" << "\n"; return std::nullopt;
        });


    /*  
        What is the behavior of the get character functions? What should it be? 

        peek() returns an unsigned char std::option type which indicates whether it's
        EOF or a valid character. If using consume() or next() when there's EOF
        char, it won't move the index anymore to the right. 

        What is the behavior of the unget function? What should it be?

        Since the index is at the end of the file + 1, it will return
        the last character in the file. When buff_idx is 0, it won't
        move any further backwards, and it will stick at 0.
    
    */
}