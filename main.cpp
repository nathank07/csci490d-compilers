#include "generator/x86/x86prog.hpp"
#include "generator/x86/x86generator.hpp"
#include "ast/ast.hpp"
#include "analyzer/analyzer.hpp"
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

    auto nodes = AbstractSyntaxTree::create(std::move(*l));
    auto table = Analyzer::analyze(nodes);
    
    for (auto& node : nodes) {
        if (node.is_error()) {
            AstError::pretty_print(node.error(), l->get_char_buff(), std::cout);
        }
    }

    if (table.node_idx) {
        std::cout << "Code tree:\n";
        AbstractSyntaxTree::print_tree(std::cout, *nodes[*table.node_idx]);
        auto prog = x86Generator::generate(std::move(nodes[*table.node_idx]), table.table);
        // std::cout << "Emitted instructions: \n";
        // x86Prog::emit_prog(prog, std::cout);
        
        std::cout << "Code size: " << prog.size() << " bytes.\n"
                  << "Code execution:\n";

        x86Prog::run_prog(prog);
        
    }
}