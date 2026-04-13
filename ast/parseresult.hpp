#pragma once
#include <variant>
#include <span>
#include "../lexer/token.hpp"
#include "asterror.hpp"

template<typename T, typename E>
struct ParseResult {
    struct Just { T value; };
    struct Nothing {};
    // Continue is functionally the same as Just{}, but it cannot be dereferenced.
    // This is essential for being able to chain ParseResults that don't particularly
    // have anything in them without implying that they do have something in them -
    // also useful for starting a new ParseResult with init()
    struct Continue {};
    struct Err { E error; };

    std::variant<Just, Nothing, Continue, Err> value;

    const std::span<const Token> consumed;
    const std::span<const Token> rest;

    ParseResult(Just j, std::span<const Token> c = {}, std::span<const Token> r = {})
        : value(std::move(j)), consumed(c), rest(r) {}

    ParseResult(Nothing n, std::span<const Token> c = {}, std::span<const Token> r = {})
        : value(n), consumed(c), rest(r) {}

    ParseResult(Err e, std::span<const Token> c = {}, std::span<const Token> r = {})
        : value(std::move(e)), consumed(c), rest(r) {}

    ParseResult(std::variant<Just, Nothing, Continue, Err> v, std::span<const Token> c, std::span<const Token> r)
        : value(std::move(v)), consumed(c), rest(r) {}

    
    ParseResult create_expr(T val) { return ParseResult(Just{std::move(val)}, consumed, rest); }


    static ParseResult init(std::span<const Token> span) { 
        if (span.empty()) return ParseResult::nothing();
        return ParseResult{ Continue{}, {}, span }; 
    }

    static ParseResult just(T val) { return Just{std::move(val)}; }
    static ParseResult nothing() { return Nothing{}; }
    static ParseResult nothing(std::span<const Token> rest) { return ParseResult{ Nothing{}, {}, rest }; }
    static ParseResult error(AstErrorType type, std::span<const Token> consumed) {
        return ParseResult{Err{AstError::create_error(type, consumed)}, consumed, {}};
    }

    static ParseResult error(AstErrorType type, std::span<const Token> consumed, auto&& expected) {
        return ParseResult{Err{AstError::create_error(type, consumed, expected)}, consumed, {}};
    }

    explicit operator bool() const {
        return is_just();
    }

    bool operator! () const {
        return !is_just();
    }

    const T& operator* () const & {
        return std::get<Just>(value).value;
    }

    T& operator* () & {
        return std::get<Just>(value).value;
    }

    T&& operator* () && {
        return std::move(std::get<Just>(value).value);
    }

    E& error() & {
        return std::get<Err>(value).error;
    }

    bool is_just() const {
        return std::holds_alternative<Just>(value);
    }

    bool is_continue() const {
        return std::holds_alternative<Continue>(value);
    }

    bool is_nothing() const {
        return std::holds_alternative<Nothing>(value);
    }

    bool is_error() const {
        return std::holds_alternative<Err>(value);
    }


    std::span<const Token> advance_1() const {
        if (consumed.empty()) return rest.subspan(0, 1);
        return std::span<const Token>(consumed.data(), consumed.size() + 1);
    }

    std::span<const Token> undo_consumed() const {
        if (consumed.empty()) return rest;
        return std::span<const Token>(consumed.data(), consumed.size() + rest.size());
    }

    template <typename P, typename S, typename F>
    ParseResult want_left_sep(P&& parse_next, S&& sep, F&& combine) {
        return take_while_just_left(parse_next(std::move(*this)),
            [&](auto&& acc, auto&& r) {
                return sep(std::move(r))
                    .then_parse([&](auto&& s) { return parse_next(std::move(s)); })
                    .then_parse([&](auto&& rhs) { return combine(std::move(acc), std::move(rhs)); });
            });
    }

    template <typename F>
    static ParseResult take_while_just_left(ParseResult&& init, F&& f) {
        return init.then_parse([&](auto&& acc) {
            return f(std::move(acc), ParseResult::init(acc.rest))
                .then_parse([&](auto&& next) {
                    return take_while_just_left(std::move(next), std::forward<F>(f));
                })
                .or_try_parse([&](auto&&) {
                    return std::move(acc);
                });
        });
    }

    template <typename P, typename S, typename F>
    ParseResult want_right_sep(P&& parse_next, S&& sep, F&& combine, AstErrorType on_missing = AstErrorType::EXPECTED_EXPRESSION) {
        return parse_next(std::move(*this))
            .then_parse([&](auto&& lhs) {
                return sep(ParseResult::init(lhs.rest))
                    .then_parse([&](auto&& after_sep) {
                        return after_sep.want_right_sep(
                            std::forward<P>(parse_next),
                            std::forward<S>(sep),
                            std::forward<F>(combine), on_missing)
                            .nothing_guard(on_missing);
                    })
                    .then_parse([&](auto&& rhs) {
                        return combine(std::move(lhs), std::move(rhs));
                    })
                    .or_try_parse([&](auto&& stopped) {
                        return stopped.create_expr(std::move(*lhs));
                    });
            });
    }

    template <typename P, typename F>
    ParseResult collect_until_tok(P&& parse_next, TokenType tok, F&& combine) {
        return want_right_sep(
            parse_next, 
            [tok](auto&& r) {
                return r.then_parse([&](auto&& c) {
                    if (c.rest.front().type == tok)
                        return ParseResult::nothing();
                    return std::move(c); }
                );
            }, 
            combine
        );
    }

    template <typename P, typename F>
    ParseResult collect_until_tok_recovering(P&& parse_next, TokenType tok, F&& combine, std::vector<ParseResult>& errors) {
        if (rest.empty() || rest.front().type == tok)
            return ParseResult::nothing(rest);

        auto attempt = parse_next(std::move(*this));

        if (attempt.is_error()) {
            errors.push_back(std::move(attempt));
            return ParseResult{Continue{}, {}, rest.subspan(std::max(attempt.size(), std::size_t{1}))}
                .collect_until_tok_recovering(
                    std::forward<P>(parse_next), tok,
                    std::forward<F>(combine), errors);
        }

        return attempt.then_parse([&](auto&& lhs) {
            return combine(std::move(lhs), 
                ParseResult{Continue{}, {}, lhs.rest}
                    .collect_until_tok_recovering(
                        std::forward<P>(parse_next), tok,
                        std::forward<F>(combine), errors));
        });
    }

    template <typename P, typename S, typename F>
    ParseResult then_want_right_sep(P&& parse_next, S&& sep, F&& combine) {
        if (!is_just() && !is_continue()) return std::move(*this);

        auto result = want_right_sep(std::forward<P>(parse_next), std::forward<S>(sep), std::forward<F>(combine));

        if (result.is_just())
            return ParseResult(
                std::move(result.value),
                merge_consumed(consumed, result.consumed),
                result.rest
            );

        if (result.is_error() && !result.consumed.empty()) {
            enrich_error(result, consumed);
            return result;
        }

        return ParseResult(Continue{}, consumed, rest);
    }

    static void enrich_error(ParseResult& result, std::span<const Token> context) {
        if (result.is_error() && !context.empty()) {
            auto& err = std::get<Err>(result.value).error;
            err.error_toks = merge_consumed(context, err.error_toks);
        }
    }

    static std::span<const Token> merge_consumed(std::span<const Token> a, std::span<const Token> b) {
        if (a.empty()) return b;
        if (b.empty()) return a;
        const Token* start = std::min(a.data(), b.data());
        const Token* end = std::max(a.data() + a.size(), b.data() + b.size());
        return std::span<const Token>(start, static_cast<std::size_t>(end - start));
    }


    /* ========================================================================

        want_tok, or_want_tok, and then_want_tok are helpers for creating ASTs
        by checking the current state of the ParseResult and acting accordingly.

        The general patterns goes like so -

          * Start from ::init(span), use want_tok/1, then chain as needed. 

          * All *_tok/2 functions will pass one ParseResult arg lambda (with the
            exception of the previously aforementioned .then_*_tok/2). This allows
            you to store the result in Just.

          * If you need to access the functions, the previously aforementioned 
            .then_*_tok/2 works - or you can always bind it with .then_parse_with.

          * or_want_tok/1 and or_want_tok/2 are used to check if the result
            returned nothing. For instance you can check if the first token like so:
                
                result
                    .want_tok(A)
                    .or_want_tok(B). 
                
            This will return ParseResult advanced once for A and B, but won't for C.

          * .then_want_tok is used after you use want_tok - this checks if *want_tok 
            found anything, then acts on it. Since then_want_tok/2 implies there's an
            expression, the lambda provides you with a the current value, and the
            rest of the ParseResult, so you can act on it.
            
          * .*_expect_tok does the same thing as *_want_tok, but if they don't match,
            it propagates an error.

          * If any of the *_toks fail, it will undo all of it's progress, and return
            nothing. This to keep the functions effectively pure - it's expected that
            if you want your contexts to be larger that you will use .*_expect_tok.
    
       ======================================================================== */

private:
    // Used with want_tok/1 and or_want_tok/1 - On match, returns the new consumed tokens
    // On no match, you can return nothing (want_tok/1), or an error (expect_tok/1)
    //
    // When you return nothing, the convention is to undo everything consumed - as the 
    // recursive descent parser expects a fresh attempt on each parse attempted
    template <typename M>
    ParseResult match(M&& match, ParseResult no_match) {
        if (is_error()) return std::move(*this);
        if (!match()) return no_match;

        return ParseResult{ Continue{}, advance_1(), rest.subspan(1) };
    }

    // Used with want_tok/2 and or_want_tok/2 - On match, returns the new 
    // consumed tokens then returns the new created ParseResult. Realistically,
    // since you only consume one token, you can only really use make_term() 
    // with this - but it should theoretically work with any function that
    // takes in one token
    //
    // On no match, it has the same behavior as init_match/2.
    template <typename M, typename F>
    ParseResult match_do(M&& match, ParseResult no_match, F&& make_this) {
        if (is_error()) return std::move(*this);
        if (!match()) return no_match;

        return make_this(ParseResult{ Continue{}, advance_1(), rest.subspan(1) });
    }

    // Used with then_want_tok/1 - This continues giving the chain of what's
    // currently inside of the ParseResult - for instance if ParseResult 
    // contains an ident, and you merge_match with tok :=, it will return
    // the initial ident.
    //
    // On no match, the convention is to undo everything it has consumed,
    // so that the chain can try something else from scratch. As an example:
    //
    // span = [LPAREN, INTEGER, RPAREN]
    //
    // node(span)                <- consumed: []       rest: [LPAREN, INTEGER, RPAREN]
    //    .want_tok(L_PAREN)     <- consumed: [LPAREN] rest: [INTEGER, RPAREN]
    //    .then_want_tok(RPAREN) <- This fails, because INTEGER is not RPAREN.
    //                              so it should create (consumed: [] rest: [LPAREN, INTEGER, RPAREN])
    //
    //    .or_want_tok(L_PAREN)  <- Able to start from scratch - creates 
    //                              consumed: [LPAREN] rest: [INTEGER, RPAREN]
    //
    //    .then_want_tok(INTEGER, make_term) <- Adds the make_term value as the Just value
    //                                          consumed: [LPAREN, INTEGER] rest: [RPAREN]
    //                                          value: Just(Term{Integer})
    //  
    //    .then_want_tok(R_PAREN) <- consumed: [LPAREN, INTEGER, RPAREN] rest: []
    //                               value: Just (Term{Integer}) 
    //
    // The final result is you get a ParseResult with the expression desired.
    template <typename M>
    ParseResult continue_match(M&& match, ParseResult no_match) {
        if (!is_just() && !is_continue()) return std::move(*this);
        if (!match()) return no_match;

        return ParseResult{ std::move(value), advance_1(), rest.subspan(1) };
    }

    // See init_match_do/3 and continue_match/2 for more info - Uses make_this
    // to continue the Just chain
    template <typename M, typename F>
    ParseResult continue_match_do(M&& match, ParseResult no_match, F&& make_this) {
        if (!is_just() && !is_continue()) return std::move(*this);

        if (!match()) return no_match;

        auto rhs = ParseResult{Continue{}, advance_1(), rest.subspan(1)};
        auto result = make_this(std::move(*this), std::move(rhs));
        enrich_error(result, consumed);
        return result;
    }

    static constexpr auto match_tok = [](auto rest, TokenType wanted) { return [rest, wanted] { return rest.front().type == wanted; }; };

    static constexpr auto convert_nothing = [](auto rest) { return ParseResult{Nothing{}, {}, rest}; };

    static constexpr auto create_error = [](auto on_fail, auto consumed, auto rest, auto expected) {
        auto err_toks = consumed.empty() && !rest.empty() ? rest.subspan(0, 1) : consumed;
        return ParseResult::error(on_fail, err_toks, expected);
    };


public:

    ParseResult want_tok(const TokenType wanted) {
        if (!is_continue()) return std::move(*this);
        return match(match_tok(rest, wanted), convert_nothing(rest));
    }

    template <typename F>
    ParseResult want_tok(const TokenType wanted, F&& make_this) {
        if (!is_continue()) return std::move(*this);
        return match_do(match_tok(rest, wanted), convert_nothing(rest), 
                             std::forward<F>(make_this));
    }

    ParseResult then_want_tok(const TokenType wanted) {
        return continue_match(match_tok(rest, wanted), convert_nothing(undo_consumed()));
    }

    template <typename F>
    ParseResult then_want_tok(const TokenType wanted, F&& make_this) {
        return continue_match_do(match_tok(rest, wanted), convert_nothing(undo_consumed()),
            std::forward<F>(make_this)
        );
    }
    
    ParseResult or_want_tok(const TokenType wanted) {
        if (!is_nothing()) return std::move(*this);
        return match(match_tok(rest, wanted), convert_nothing(rest));
    }

    template <typename F>
    ParseResult or_want_tok(const TokenType wanted, F&& make_this) {
        if (!is_nothing()) return std::move(*this);
        return match_do(match_tok(rest, wanted), convert_nothing(rest), 
                             std::forward<F>(make_this));
    }

    ParseResult expect_tok(const TokenType expected, AstErrorType on_fail = AstErrorType::EXPECTED_TOK) {
        if (!is_continue()) return std::move(*this);
        return match(match_tok(rest, expected), create_error(on_fail, consumed, rest, expected));
    }

    template <typename F>
    ParseResult expect_tok(const TokenType expected, F&& make_this, AstErrorType on_fail = AstErrorType::EXPECTED_TOK) {
        if (!is_continue()) return std::move(*this);
        return match_do(match_tok(rest, expected), create_error(on_fail, consumed, rest, expected),
            std::forward<F>(make_this));
    }

    ParseResult then_expect_tok(const TokenType expected, AstErrorType on_fail = AstErrorType::EXPECTED_TOK) {
        return continue_match(match_tok(rest, expected),
            create_error(on_fail, consumed, rest, expected));
    }

    template <typename F>
    ParseResult then_expect_tok(const TokenType expected, F&& make_this, AstErrorType on_fail = AstErrorType::EXPECTED_TOK) {
        return continue_match_do(match_tok(rest, expected),
            create_error(on_fail, consumed, rest, expected), std::forward<F>(make_this));
    }

    ParseResult or_expect_tok(const TokenType expected, AstErrorType on_fail = AstErrorType::EXPECTED_TOK) {
        if (!is_nothing()) return std::move(*this);
        return match(match_tok(rest, expected), create_error(on_fail, consumed, rest, expected));
    }

    template <typename F>
    ParseResult or_expect_tok(const TokenType expected, F&& make_this, AstErrorType on_fail = AstErrorType::EXPECTED_TOK) {
        if (!is_nothing()) return std::move(*this);
        return match_do(match_tok(rest, expected), create_error(on_fail, consumed, rest, expected),
            std::forward<F>(make_this));
    }

    // ident matching helpers — like *_tok but match IDENTIFIER tokens with a specific value
private:
    auto rest_is_ident(const std::string& name) const {
        return [rest = this->rest, &name] {
            return rest.front().type == TokenType::IDENTIFIER
                && std::holds_alternative<TokenIdentifier>(rest.front().data)
                && std::get<TokenIdentifier>(rest.front().data).value == name;
        };
    }

    static constexpr auto create_i_err = [](auto consumed, auto rest, auto name) {
        auto err_toks = consumed.empty() && !rest.empty() ? rest.subspan(0, 1) : consumed;
        return ParseResult::error(AstErrorType::EXPECTED_IDENT, err_toks, name);
    };

public:

    ParseResult want_ident(const std::string& name) {
        if (!is_continue()) return std::move(*this);
        return match(rest_is_ident(name), convert_nothing(rest));
    }

    template <typename F>
    ParseResult want_ident(const std::string& name, F&& make_this) {
        if (!is_continue()) return std::move(*this);
        return match_do(rest_is_ident(name), convert_nothing(rest),
            std::forward<F>(make_this));
    }

    ParseResult or_want_ident(const std::string& name) {
        if (!is_nothing()) return std::move(*this);
        return match(rest_is_ident(name), convert_nothing(rest));
    }

    template <typename F>
    ParseResult or_want_ident(const std::string& name, F&& make_this) {
        if (!is_nothing()) return std::move(*this);
        return match_do(rest_is_ident(name), convert_nothing(rest),
            std::forward<F>(make_this));
    }

    ParseResult then_want_ident(const std::string& name) {
        return continue_match(rest_is_ident(name), convert_nothing(undo_consumed()));
    }

    template <typename F>
    ParseResult then_want_ident(const std::string& name, F&& make_this) {
        return continue_match_do(rest_is_ident(name), convert_nothing(undo_consumed()),
            std::forward<F>(make_this));
    }

    ParseResult expect_ident(const std::string& name) {
        if (!is_continue()) return std::move(*this);
        return match(rest_is_ident(name), create_i_err(consumed, rest, name));
    }

    template <typename F>
    ParseResult expect_ident(const std::string& name, F&& make_this) {
        if (!is_continue()) return std::move(*this);
        return match_do(rest_is_ident(name), create_i_err(consumed, rest, name),
            std::forward<F>(make_this));
    }

    ParseResult then_expect_ident(const std::string& name) {
        return continue_match(rest_is_ident(name), create_i_err(consumed, rest, name));
    }

    template <typename F>
    ParseResult then_expect_ident(const std::string& name, F&& make_this) {
        return continue_match_do(rest_is_ident(name), create_i_err(consumed, rest, name),
            std::forward<F>(make_this));
    }

    ParseResult or_expect_ident(const std::string& name) {
        if (!is_nothing()) return std::move(*this);
        return match(rest_is_ident(name), create_i_err(consumed, rest, name));
    }

    template <typename F>
    ParseResult or_expect_ident(const std::string& name, F&& make_this) {
        if (!is_nothing()) return std::move(*this);
        return match_do(rest_is_ident(name), create_i_err(consumed, rest, name),
            std::forward<F>(make_this));
    }

    template<typename F>
    ParseResult or_try_parse(F&& parser) {
        if (!is_nothing()) 
            return std::move(*this);
    
        return parser(std::move(*this));
    }

    template <typename F>
    ParseResult then_parse(F&& parser) {
        if (is_just() || is_continue()) {
            auto result = parser(std::move(*this));
            if (!result.is_just()) return convert_nothing(rest);
            return ParseResult(
                std::move(result.value), 
                merge_consumed(consumed, result.consumed), 
                result.rest
            );
        }
        return std::move(*this);
    }

    template <typename F, typename P>
    ParseResult then_parse_with(P&& parser, F&& combine) {
        if (is_just()) {
            auto rhs = parser(ParseResult{Continue{}, consumed, rest});
            if (!rhs.is_just()) return convert_nothing(undo_consumed());

            auto result = combine(std::move(*this), std::move(rhs));
            if (!result.is_just()) return result;
            
            return ParseResult(
                Just{std::move(*result)}, 
                merge_consumed(consumed, result.consumed), 
                result.rest
            );
        }
        return std::move(*this);
    }

    template<typename F>
    ParseResult on_fail(F&& next) {
        if (is_error()) {
            return next(std::move(*this));
        }
        return std::move(*this);
    }

    ParseResult nothing_guard(AstErrorType err_type) {
        if (is_nothing()) {
            return ParseResult{Err{AstError::create_error(err_type, rest.subspan(0, 1))}, {}, rest};
        }
        return std::move(*this);
    }

    std::size_t size() {
        return consumed.size();
    }
};