#include "ast.hpp"
#include "../lexer/lexer.hpp"
#include "asterror.hpp"
#include "expression.hpp"
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

    std::cout << "node_v len" << node_v.size() << "\n";
    
    for (auto& node : node_v) {
        if (!node.is_error()) {
            AbstractSyntaxTree::print_tree(std::cout, *node);
        } else {
            node.map_err([&](auto&& err) {
                AstError::pretty_print(err, l->get_char_buff(), std::cout);
                return NodeResult::Err(err);
            });
        }
    }
}