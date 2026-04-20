#include "generator/midos/midosGenerator.hpp"
#include "ast/ast.hpp"
#include "analyzer/analyzer.hpp"
#include <cstring>
#include <fstream>
#include <iostream>

int main(int argc, char** argv) {

    if (argc != 4 || std::strcmp(argv[2], "-o") != 0) {
        std::cerr << "Usage: " << argv[0] << " <input> -o <output>\n";
        return 1;
    }

    auto l = Lexer::create(std::string(argv[1]), Lexer::OnTokenError::HALT);

    if (!l) {
        std::cerr << l.error().message << "\n";
        return 1;
    }

    std::ofstream out(argv[3]);
    if (!out) {
        std::cerr << "Error: could not open output file '" << argv[3] << "'\n";
        return 1;
    }

    auto nodes = AbstractSyntaxTree::create(std::move(*l));
    auto table = Analyzer::analyze(nodes);

    for (auto& node : nodes) {
        if (node.is_error()) {
            AstError::pretty_print(node.error(), l->get_char_buff(), std::cerr);
        }
    }

    if (table.node_idx) {
        midosGenerator::generate(std::move(nodes[*table.node_idx]), table.table, out);
    }
}
