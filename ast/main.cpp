#include "ast.hpp"
#include "../lexer/lexer.hpp"
#include <iostream>

int main(int argc, char** argv) {

    if (argc != 2) {
        std::cerr << "Usage: " <<  argv[0] << " <filepath>\n";
        return 1;
    }

    auto l = Lexer::create(std::string(argv[1]), Lexer::OnTokenError::HALT);

    if (!l) {
        std::cout << l.error().message << "\n";
        return 1;
    }

    AbstractSyntaxTree a;
    auto nodeOpt = a.create(*l);
    if (nodeOpt) {
        a.print_tree(std::cout, *nodeOpt);
    } else {
        std::cout << "failed to parse";
    }
}