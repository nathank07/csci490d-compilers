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

    bool has_one_expr = false;

    for (auto& node : nodes) {
        if (!node.is_error()) {
            if (has_one_expr) {
                std::cout << "Found more than one expression <";
                auto& expr_span = (*node)->token_span;
                auto& first = expr_span.front();
                auto& last = expr_span.back();
                for (std::size_t ln = first.line_number; ln <= last.line_number; ++ln) {
                    auto line = l->get_char_buff().get_line(ln);
                    if (!line) continue;
                    std::size_t start = (ln == first.line_number) ? first.column_number - 1 : 0;
                    std::size_t end = (ln == last.line_number) ? last.column_number - 1 + last.get_token_width() : line->size();
                    std::cout << line->substr(start, end - start);
                }
                std::cout << "> omitting..." << std::endl;
            }
            has_one_expr = true;
            midosGenerator::generate(prog, std::move(*node));
        } else {
            node.map_err([&](auto&& err) {
                AstError::pretty_print(err, l->get_char_buff(), std::cerr);
                return NodeResult::Err(err);
            });
        }
    }
}
