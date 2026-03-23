#pragma once
#include "../lexer/token.hpp"

enum class AstErrorType {
    MISMATCH_PAREN,
    FAILED_TO_PARSE_SYMBOL,
    EMPTY_PARENS,
    UNEXPECTED_EOF
};

struct AstError {
    AstErrorType type;
    std::string message;
    std::size_t skip_x_tok;
    Token offending_token;
    std::optional<Token> begin_tok = std::nullopt;

    static AstError mismatched_bracket(Token begin_tok, Token end_of_expr) {
        return AstError{
            AstErrorType::MISMATCH_PAREN, 
            "Mismatched parenthesis", 
            1, 
            end_of_expr,
            begin_tok
        };
    }

    static AstError bad_symbol(Token bad_symbol) {
        if (bad_symbol.type == TokenType::END_OF_FILE)
            return unexpected_eof(bad_symbol);

        return AstError{
            AstErrorType::FAILED_TO_PARSE_SYMBOL, 
            "Bad symbol " + bad_symbol.get_type_string(), 
            1, 
            bad_symbol
        };
    }

    static AstError unexpected_eof(Token eof) {
        return AstError{
            AstErrorType::UNEXPECTED_EOF, 
            "Unexpected EOF while parsing", 
            1, 
            eof
        };
    }

    static AstError empty_parens(Token empty_left, Token r_paren) {
        return AstError{
            AstErrorType::EMPTY_PARENS, 
            "Empty parens", 
            2,
            empty_left,
            r_paren
        };
    }

    static AstError empty_parens(const AstError& empty_parens_err) {
        return AstError{
            AstErrorType::EMPTY_PARENS, 
            "Empty parens", 
            empty_parens_err.skip_x_tok + 1,
            empty_parens_err.offending_token,
            empty_parens_err.begin_tok
        };
    }

    static void pretty_print(const AstError& err, const Input& i, std::ostream& o) {
        o << "\n";
        switch (err.type) {
            case AstErrorType::MISMATCH_PAREN: pretty_print_mismatch(err, i, o); return;
            case AstErrorType::FAILED_TO_PARSE_SYMBOL: pretty_print_bad_symbol(err, i, o); return;
            case AstErrorType::UNEXPECTED_EOF: pretty_print_unexpected_eof(err, i, o); return;
            default: {
                auto& ot = err.offending_token;

                o << "Error: " << err.message << " at " 
                  << ot.line_number << ":"
                  << ot.column_number << std::endl;

                print_line(ot.line_number, i, o);
                pretty_print_arrow(ot.column_number, o);
            }
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

    static void pretty_print_mismatch(const AstError& err, const Input& in, std::ostream& o) {
        auto& expr_end = err.offending_token;
        auto& bt = *err.begin_tok;
        auto col = expr_end.get_token_width() + expr_end.column_number;
        
        o << "Expected ')' at " << expr_end.line_number << ":" << col << "\n";

        print_line(expr_end.line_number, in, o);

        pretty_print_arrow(bt.column_number, col, o);

        o << "\n";
    }

    static void pretty_print_bad_symbol(const AstError& err, const Input& in, std::ostream& o) {
        auto& ot = err.offending_token;
        o << "Unexpected symbol at " << ot.line_number << ":" << ot.column_number << "\n";

        print_line(ot.line_number, in, o);

        pretty_print_arrow(ot.column_number, o);

        o << "\n";
    }

    static void pretty_print_unexpected_eof(const AstError& err, const Input& in, std::ostream& o) {
        auto& ot = err.offending_token;
        o << "Unexpected EOF parsing " << ot.line_number << ":" << ot.column_number << "\n";

        print_line(ot.line_number - 1, in, o);
        print_line(ot.line_number, in, o);

        o << "\n";
    }

};