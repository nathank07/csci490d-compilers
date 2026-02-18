#include "generator/x86prog.hpp"
#include "generator/x86generator.hpp"
#include "ast/ast.hpp"
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
    auto node_v = std::move(a.create(std::move(*l)));
    auto expressions = a.unwrap_valid_nodes(node_v);

    for (auto& expr : expressions) {
        std::cout << "value: " << x86Prog::run_prog(x86Generator::generate(std::move(expr))) << "\n";
    }
    
}