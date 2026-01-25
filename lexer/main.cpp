#include "lexer.hpp"
#include <iostream>

int main(int argc, char** argv) {
    auto l = Lexer::create(std::string(argv[1]), Lexer::OnTokenError::CONTINUE);

    if (!l) {
        std::cout << l.error().message;
    }

    for (auto t : (*l).get_tokens()) {
        std::cout << t.get_type_string() << " at " << t.line_number << ":" << t.column_number << "\n";
    }

    for (auto e : (*l).get_invalid_tokens()) {
        std::cout << e.message << " " << e.line_number << ":" << e.column_number << "\n";
    }
}