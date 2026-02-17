#include "x86prog.hpp"
#include "../ast/ast.hpp"
#include "x86generator.hpp"
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

    x86Prog p;
    x86Generator gen;

    gen.evaluate_expression(p, std::move(expressions[0]));

    // auto test = [](x86Prog& p) {
        // p.mov(Register::EAX, Immediate{-7});
    // };

    // auto r = x86Prog::run_prog([&](x86Prog p) {
    //     p.mov(Register::EAX, Immediate{1});
    //     // p.push(Register::EAX);
    //     p.ret();
    // });
    p.add_instructions([&](x86Prog p) { p.ret(); });

    std::cout << "value: " << p.execute() << "\n";
}