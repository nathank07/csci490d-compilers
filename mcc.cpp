#include "generator/midos/midosGenerator.hpp"
#include "ast/ast.hpp"
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

    auto nodes = std::move(AbstractSyntaxTree::create(std::move(*l)));

    midOsProg prog{out};

    for (auto& node : nodes) {
        if (!node.is_error()) {
            midosGenerator::generate(prog, std::move(*node));
        } else {
            node.map_err([&](auto&& err) {
                AstError::pretty_print(err, l->get_char_buff(), std::cerr);
                return NodeResult::Err(err);
            });
        }
    }
}
