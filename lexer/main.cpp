#include "lexer.hpp"
#include <iostream>

int main() {
    auto l = Lexer::create("test108", Lexer::OnTokenError::CONTINUE);

    if (!l) {
        std::cout << l.error().message;
    }

    for (auto t : (*l).get_tokens()) {
        std::cout << t.get_type_string() << "\n";
    }
}