#include "lexer.hpp"
#include <iostream>

int main() {
    auto l = Lexer::create("test001", Lexer::OnTokenError::CONTINUE);

    if (!l) {
        std::cout << l.error().message;
    }

    for (auto t : (*l).get_tokens()) {
        std::cout << t.line_number << ":" << t.column_number << " " << t.get_type_string() << "\n";
    }

    for (auto e : (*l).get_invalid_tokens()) {
        std::cout << e.message << " " << e.line_number << ":" << e.column_number << "\n";
    }
}