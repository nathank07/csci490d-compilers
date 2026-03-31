#include "x86/x86prog.hpp"
#include "../ast/ast.hpp"
#include "../analyzer/analyzer.hpp"
#include "x86/x86generator.hpp"
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

    auto node_v = std::move(AbstractSyntaxTree::create(std::move(*l)));
    auto table = Analyzer::analyze(node_v);


    std::cout << "value: " << x86Prog::run_prog(x86Generator::generate(std::move(*table.node_loc), table.table)) << "\n";    
}