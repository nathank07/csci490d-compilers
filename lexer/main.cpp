#include "lexer.hpp"
#include <iostream>

int main() {
    auto l = Lexer::create("test001");

    if (!l) {
        std::cout << l.error().message;
    } else {
        l->output_tokens();
    }
}