#pragma once
#include "../lexer/token.hpp"
#include <span>
#include <variant>

enum class AstErrorType {
    EXPECTED_STATEMENT,
    EXPECTED_EXPRESSION,
    EXPECTED_TOK,
    EXPECTED_IDENT,
    NO_NESTED_UNARIES
};

struct AstError {
    AstErrorType type;
    std::span<const Token> error_toks;
    std::variant<std::monostate, TokenType, std::string> expected = std::monostate{};
    
    static AstError create_error(AstErrorType err, std::span<const Token> toks) {
        return { err, toks };
    }

    static AstError create_error(AstErrorType err, std::span<const Token> toks, std::variant<std::monostate, TokenType, std::string> expected) {
        return { err, toks, expected };
    }

    static void pretty_print(const AstError& err, const Input& i, std::ostream& o) {
        if (err.error_toks.empty()) {
            o << "\nError: " << static_cast<int>(err.type) << "\n";
            return;
        }
        o << "\n";
        switch (err.type) {
            // case AstErrorType::MISMATCH_PAREN: pretty_print_mismatch(err, i, o); return;
            // case AstErrorType::FAILED_TO_PARSE_SYMBOL: pretty_print_bad_symbol(err, i, o); return;
            // case AstErrorType::UNEXPECTED_EOF: pretty_print_unexpected_eof(err, i, o); return;
            case AstErrorType::EXPECTED_EXPRESSION: err.pretty_print_bad_expr(i, o); return;
            case AstErrorType::EXPECTED_TOK: err.pretty_print_bad_tok(i, o); return;
            case AstErrorType::EXPECTED_STATEMENT: err.pretty_print_bad_stmt(i, o); return;
            case AstErrorType::NO_NESTED_UNARIES: err.pretty_print_unsupported_unary(i, o); return;
            default: o << static_cast<int>(err.type); return;
        }
        o << std::endl;
    }

private:

    static void pretty_print_arrow(std::size_t pos, std::ostream& o) {
        
        for (std::size_t i = 0; i < pos - 1; i++) {
            if (i == pos) { o << "^"; }
            else { o << "-"; }
        }

        o << "^\n";
    }

    static void pretty_print_arrow(std::size_t pos1, std::size_t pos2, std::ostream& o) {
        for (std::size_t i = 0; i < pos2; i++) {
            if (i == pos1 - 1 || i == pos2 - 1) { 
                o << "^";
            }
            else { 
                o << (i < pos1 - 1 ? "-" : " "); 
            }
        }
        
        o << "\n";

        for (std::size_t i = 0; i < pos2 - 1; i++) {
            o << "-";
        }

        o << "╯\n";
    }

    static void print_line(std::size_t line, const Input& in, std::ostream& o) {
        auto l = in.get_line(line);

        if (!l) {
            o << "<error printing line>\n";
            return;
        }

        o << *l;

        if (l->empty() || l->back() != '\n') {
            o << "\n";
        }
    }

    void print_span_context(const Input& in, std::ostream& o) const {

        auto first = error_toks.front();
        auto last = error_toks.back();

        for (std::size_t i = first.line_number; i < last.line_number; i++) {

            auto try_line = in.get_line(i);

            if (!try_line) {
                o << "<error printing line>\n";
                continue;
            }

            auto line = *try_line;
            auto len = line.length();
            if (!line.empty() && line.back() == '\n') len--;

            o << line;
            if (line.empty() || line.back() != '\n') o << "\n";
            o << std::string(len, '~') << "\n";
        }
    }

    void pretty_print_bad_expr(const Input& in, std::ostream& o) const {
        auto expr_end = error_toks.back();
        auto col = expr_end.column_number;

        o << "Expected expression"
          << " at " << expr_end.line_number << ":" << col << "\n\n";

        print_span_context(in, o);
        print_line(expr_end.line_number, in, o);
        pretty_print_arrow(col, o);
    }

    void pretty_print_bad_stmt(const Input& in, std::ostream& o) const {
        auto expr_end = error_toks.back();
        auto col = expr_end.column_number;

        o << "Expected statement"
          << " at " << expr_end.line_number << ":" << col << "\n\n";
        
        print_span_context(in, o);
        print_line(expr_end.line_number, in, o);
        pretty_print_arrow(col, o);
    }

    void pretty_print_bad_tok(const Input& in, std::ostream& o) const {

        auto expr_end = error_toks.back();
        auto col = expr_end.get_token_width() + expr_end.column_number;


        o << "Expected \'" << Token::get_token_literal(std::get<TokenType>(expected)) << "\'"
          << " at " << expr_end.line_number << ":" << col << "\n\n";

        print_span_context(in, o);
        print_line(expr_end.line_number, in, o);
        pretty_print_arrow(col, o);
    }

    void pretty_print_unsupported_unary(const Input& in, std::ostream& o) const {
        pretty_print_bad_expr(in, o);
        o << "\nHint: Did you try to use a nested unary? They are not supported in this language.\n\n";
    }

};