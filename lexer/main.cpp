#include "lexer.hpp"
#include "token.hpp"
#include "error.hpp"
#include <algorithm>
#include <iostream>
#include <variant>
#include <vector>

// https://en.cppreference.com/w/cpp/utility/variant/visit.html
template<class... Ts>
struct match : Ts... { using Ts::operator()...; };

int main(int argc, char** argv) {

    if (argc != 2) {
        std::cerr << "Usage: " <<  argv[0] << " <filepath>\n";
        return 1;
    }

    auto l = Lexer::create(std::string(argv[1]), Lexer::OnTokenError::CONTINUE);

    if (!l) {
        std::cout << l.error().message << "\n";
        return 1;
    }

    std::vector<std::variant<Token, LexerError>> tokens_and_errors;

    for (auto &t : (*l).get_tokens()) {
        tokens_and_errors.push_back(t);
    }

    for (auto &e : (*l).get_invalid_tokens()) {
        tokens_and_errors.push_back(e);
    }

    std::sort(tokens_and_errors.begin(), tokens_and_errors.end(), [](auto const& a, auto const& b) {
        auto a_line = std::visit([](auto const& v) { return v.line_number; }, a);
        auto a_col  = std::visit([](auto const& v) { return v.column_number; }, a);
        auto b_line = std::visit([](auto const& v) { return v.line_number; }, b);
        auto b_col  = std::visit([](auto const& v) { return v.column_number; }, b);

        if (a_line != b_line) 
            return a_line < b_line;

        return a_col < b_col;
    });

    for (auto t : tokens_and_errors) {
        const auto visitor = match {
            [](Token v) {
                std::cout << v.get_type_string() << " at " << v.line_number << ":" << v.column_number << "\n";
            },
            [](LexerError e) {
                std::cout << e.message << " " << e.line_number << ":" << e.column_number << "\n";
            }
        };
        std::visit(visitor, t);
    }
}