#include "generator/x86/x86prog.hpp"
#include "generator/x86/x86generator.hpp"
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

    auto nodes = std::move(AbstractSyntaxTree::create(std::move(*l)));

    for (auto& node : nodes) {
        if (!node.is_error()) {
            std::cout << "Code tree:\n";
            AbstractSyntaxTree::print_tree(std::cout, *node);
            auto p = x86Prog::run_prog_bytes(x86Generator::generate(std::move(*node)));
            std::cout << "Code size: " << p.second << " bytes.\n"
                      << "Code execution:\n" << p.first << "\n\n"; 
        } else {
            node.map_err([&](auto&& err) {
                AstError::pretty_print(err, l->get_char_buff(), std::cout);
                return NodeResult::Err(err);
            });
        }
    }
}