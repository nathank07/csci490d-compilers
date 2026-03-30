#pragma once
#include "../lexer/token.hpp"
#include <span>
#include <variant>

enum class AstErrorType {
    EXPECTED_STATEMENT,
    EXPECTED_EXPRESSION,
    EXPECTED_TOK,
    EXPECTED_IDENT,
    NO_NESTED_UNARIES,
    COMMA_OR_RPAREN,
    UNRECOGNIZED_IDENT,
    UNRECOGNIZED_TYPE,
    VAR_DEFINED
};

struct AstError {
    AstErrorType type;
    std::span<const Token> error_toks;
    std::variant<std::monostate, TokenType, std::string> expected = std::monostate{};
    std::optional<Token> subject = std::nullopt;

    static AstError create_error(AstErrorType err, std::span<const Token> toks) {
        return { err, toks };
    }

    static AstError create_error(AstErrorType err, std::span<const Token> toks, std::variant<std::monostate, TokenType, std::string> expected) {
        return { err, toks, expected };
    }

    static AstError create_error(AstErrorType err, std::span<const Token> toks, Token subject) {
        return { err, toks, std::monostate{}, subject };
    }

    static void pretty_print(const AstError& err, const Input& i, std::ostream& o) {
        if (err.error_toks.empty()) {
            o << "\nError: " << static_cast<int>(err.type) << "\n";
            return;
        }
        o << "\n";
        switch (err.type) {
            case AstErrorType::EXPECTED_EXPRESSION: err.pretty_print_bad_expr(i, o); return;
            case AstErrorType::EXPECTED_TOK: err.pretty_print_bad_tok(i, o); return;
            case AstErrorType::EXPECTED_STATEMENT: err.pretty_print_bad_stmt(i, o); return;
            case AstErrorType::NO_NESTED_UNARIES: err.pretty_print_unsupported_unary(i, o); return;
            case AstErrorType::COMMA_OR_RPAREN: err.pretty_print_comma_or_rparen(i, o); return;
            case AstErrorType::UNRECOGNIZED_IDENT: err.pretty_print_bad_ident(i, o); return;
            case AstErrorType::UNRECOGNIZED_TYPE: err.pretty_print_bad_type(i, o); return;
            case AstErrorType::VAR_DEFINED: err.pretty_print_var_defined(i, o); return;
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

    void print_span_context(const Input& in, std::ostream& o, bool include_last_line) const {

        auto first = error_toks.front();
        auto last = error_toks.back();

        for (std::size_t i = first.line_number; i <= last.line_number; i++) {

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

            if (i == last.line_number) {
                if (!include_last_line) return;
                auto start = (i == first.line_number) ? first.column_number - 1 : std::size_t{0};
                auto end = last.column_number - 1 + last.get_token_width();
                if (end > len) end = len;
                o << std::string(start, ' ') << std::string(end - start, '~') << "\n";
            } else {
                o << std::string(len, '~') << "\n";
            }
        }
    }

    void pretty_print_bad_expr(const Input& in, std::ostream& o) const {
        auto expr_end = error_toks.back();
        auto col = expr_end.column_number;

        print_span_context(in, o, true);
        o << "\nExpected expression while reading statement, read '" 
          << expr_end.get_token_literal() << "' ("  
          << expr_end.line_number << ":" << col << ")\n\n";
    }

    void pretty_print_bad_stmt(const Input& in, std::ostream& o) const {
        auto expr_end = error_toks.back();
        auto col = expr_end.column_number;

        o << "Expected beginning of statement at "
          << expr_end.line_number << ":" << col << ", read '"
          << expr_end.get_token_literal() << "' instead\n\n";
        
        print_span_context(in, o, false);
        pretty_print_arrow(col, o);
    }

    void pretty_print_bad_tok(const Input& in, std::ostream& o) const {

        auto expr_end = error_toks.back();
        auto col = expr_end.get_token_width() + expr_end.column_number;


        o << "Expected \'" << Token::get_token_literal(std::get<TokenType>(expected)) << "\'"
          << " at " << expr_end.line_number << ":" << col << "\n\n";

        print_span_context(in, o, true);
        pretty_print_arrow(col, o);
    }

    void pretty_print_unsupported_unary(const Input& in, std::ostream& o) const {
        pretty_print_bad_expr(in, o);
        o << "\nHint: Did you try to use a nested unary? They are not supported in this language.\n\n";
    }

    void pretty_print_comma_or_rparen(const Input& in, std::ostream& o) const {
        auto expr_end = error_toks.back();
        auto col = expr_end.get_token_width() + expr_end.column_number;
        o << "Expected ',' or ')' at " << expr_end.line_number << ":" << col << "\n\n";
        print_span_context(in, o, true);
        pretty_print_arrow(col, o);
    }

    void pretty_print_bad_ident(const Input& in, std::ostream& o) const {
        auto term = subject.value_or(error_toks.back());
        auto col = term.column_number;
        o << "Undeclared variable '" << term.get_token_literal() << "' at "
          << term.line_number << ":" << term.column_number << "\n\n";

        print_span_context(in, o, true);
        pretty_print_arrow(col, o);
    }

    void pretty_print_bad_type(const Input& in, std::ostream& o) const {
        auto term = error_toks.front();
        auto col = term.column_number;
        o << "Unrecognized type '" << term.get_token_literal() << "' at "
          << term.line_number << ":" << term.column_number << "\n\n";

        print_span_context(in, o, true);
        pretty_print_arrow(col, o);
    }

    void pretty_print_var_defined(const Input& in, std::ostream& o) const {
        auto term = error_toks.subspan(1).front();
        auto col = term.column_number;
        o << "Attempted to declare '" << term.get_token_literal() << "' at "
          << term.line_number << ":" << term.column_number << ", but it is already defined.\n\n";

        print_span_context(in, o, true);
        pretty_print_arrow(col, o);
    }
};