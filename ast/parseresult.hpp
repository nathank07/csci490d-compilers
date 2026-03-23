#pragma once
#include <variant>
#include <optional>
#include <span>
#include "../lexer/token.hpp"
#include "asterror.hpp"

template<typename T, typename E>
struct ParseResult {
    struct Just { T value; };
    struct Nothing {};
    struct Err { E error; };

    std::variant<Just, Nothing, Err> value;

    const std::span<const Token> consumed;
    const std::span<const Token> rest;

    ParseResult(Just j, std::span<const Token> consumed = {}, std::span<const Token> rest = {})
        : value(std::move(j)), consumed(consumed), rest(rest) {}

    ParseResult(Nothing n, std::span<const Token> consumed = {}, std::span<const Token> rest = {})
        : value(n), consumed(consumed), rest(rest) {}

    ParseResult(Err e, std::span<const Token> consumed = {}, std::span<const Token> rest = {})
        : value(std::move(e)), consumed(consumed), rest(rest) {}
        
    ParseResult(std::variant<Just, Nothing, Err> v, std::span<const Token> consumed, std::span<const Token> rest)
        : value(std::move(v)), consumed(consumed), rest(rest) {}

    
    ParseResult create_expr(T val) { return ParseResult(Just{std::move(val)}, consumed, rest); }

    static ParseResult just(T val) { return Just{std::move(val)}; }
    static ParseResult nothing() { return Nothing{}; }
    static ParseResult nothing(std::span<const Token> rest) { return ParseResult{ Nothing{}, {}, rest }; }
    static ParseResult error(E err) { return Err{std::move(err)}; }

    explicit operator bool() const {
        return std::holds_alternative<Just>(value);
    }

    bool operator! () const {
        return !std::holds_alternative<Just>(value);
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


    static std::span<const Token> merge_consumed(std::span<const Token> a, std::span<const Token> b) {
        if (a.empty()) return b;
        if (b.empty()) return a;
        const Token* start = std::min(a.data(), b.data());
        const Token* end = std::max(a.data() + a.size(), b.data() + b.size());
        return std::span<const Token>(start, static_cast<std::size_t>(end - start));
    }

    template <typename F1, typename F2, typename F3>
    ParseResult want_left_sep(F1&& parse_next, F2&& sep, F3&& combine) {
        return take_while_just_left(parse_next(std::move(*this)),
            [&](auto&& acc, auto&& rest) {
                return sep(std::move(rest))
                    .and_then([&](auto&& s) { return parse_next(std::move(s)); })
                    .and_then([&](auto&& rhs) { return combine(std::move(acc), std::move(rhs)); });
            });
    }

    template <typename F>
    static ParseResult take_while_just_left(ParseResult&& init, F&& f) {
        return init.and_then([&](auto&& acc) {
            return f(std::move(*acc), ParseResult::nothing(acc.rest))
                .and_then([&](auto&& next) {
                    return take_while_just_left(std::move(next), std::forward<F>(f));
                })
                .or_else([&](auto&& stopped) {
                    // Nothing means "done folding" in this context rather
                    // than "didn't find anything" - so return a just value
                    return stopped.create_expr(std::move(*acc));
                });
        });
    }

    template <typename P, typename S, typename F>
    ParseResult want_right_sep(P&& parse_next, S&& sep, F&& combine) {
        return parse_next(std::move(*this))
            .and_then([&](auto&& lhs) {
                return sep(ParseResult::nothing(lhs.rest))
                    .and_then([&](auto&& after_sep) {
                        return after_sep.want_right_sep(
                            std::forward<P>(parse_next),
                            std::forward<S>(sep),
                            std::forward<F>(combine));
                    })
                    .and_then([&](auto&& rhs) {
                        return combine(std::move(*lhs), std::move(rhs));
                    })
                    .or_else([&](auto&& stopped) {
                        return stopped.create_expr(std::move(*lhs));
                    });
            });
    }

    ParseResult want_tok(const TokenType wanted) {
        if (std::holds_alternative<Err>(value)) return std::move(*this);
        if (rest.front().type == wanted) {
            auto new_consumed = merge_consumed(consumed, rest.subspan(0, 1));
            // If you're using want_tok, it's assumed that you don't care
            // about the Expression of the token (using it for syntax)
            return ParseResult{Just{T{}},
                new_consumed,
                rest.subspan(1)};
        }

        return std::move(*this);
    }

    template <typename F>
    ParseResult want_tok(const TokenType wanted, F&& make_this) {
        if (std::holds_alternative<Err>(value)) return std::move(*this);
        if (rest.front().type == wanted) {
            auto new_consumed = merge_consumed(consumed, rest.subspan(0, 1));;
            return ParseResult{Just{make_this(rest.front())},
                new_consumed,
                rest.subspan(1)};
        }

        return std::move(*this);
    }

    ParseResult then_want_tok(const TokenType wanted) {
        if(std::holds_alternative<Just>(value)) return want_tok(wanted);
            
        return std::move(*this);
    }

    template <typename F>
    ParseResult then_want_tok(const TokenType expected, F&& make_this) {
        if(std::holds_alternative<Just>(value)) 
            return want_tok(expected, make_this);

        return std::move(*this);
    }
    
    ParseResult or_want_tok(const TokenType wanted) {
        if(std::holds_alternative<Nothing>(value)) return want_tok(wanted);
            
        return std::move(*this);
    }

    template <typename F>
    ParseResult or_want_tok(const TokenType expected, F&& make_this) {
        if(std::holds_alternative<Nothing>(value)) 
            return want_tok(expected, make_this);

        return std::move(*this);
    }

    ParseResult expect_tok(const TokenType expected) {
        if (std::holds_alternative<Err>(value)) return std::move(*this);
        if (rest.front().type == expected) {
            auto new_consumed = merge_consumed(consumed, rest.subspan(0, 1));;
            return ParseResult{Just{std::move(std::get<Just>(value).value)},
                new_consumed,
                rest.subspan(1)};
        }

        return std::move(ParseResult::error(AstError::bad_symbol(rest.front())));
    }

    template <typename F>
    ParseResult expect_tok(const TokenType expected, F&& make_this) {
        if (std::holds_alternative<Err>(value)) return std::move(*this);
        if (rest.front().type == expected) {
            auto new_consumed = merge_consumed(consumed, rest.subspan(0, 1));;
            return ParseResult{Just{make_this(rest.front())},
                new_consumed,
                rest.subspan(1)};
        }

        return std::move(ParseResult::error(AstError::bad_symbol(rest.front())));
    }

    ParseResult then_expect_tok(const TokenType expected) {
        if(std::holds_alternative<Just>(value)) return expect_tok(expected);

        return std::move(*this);
    }

    template <typename F>
    ParseResult then_expect_tok(const TokenType expected, F&& make_this) {
        if(std::holds_alternative<Just>(value)) 
            return expect_tok(expected, make_this);

        return std::move(*this);
    }

    ParseResult or_expect_tok(const TokenType expected) {
        if(std::holds_alternative<Nothing>(value)) return expect_tok(expected);

        return std::move(*this);
    }

    template <typename F>
    ParseResult or_expect_tok(const TokenType expected, F&& make_this) {
        if(std::holds_alternative<Nothing>(value)) 
            return expect_tok(expected, make_this);

        return std::move(*this);
    }

    template <typename F>
    ParseResult and_then(F&& next) {
        if (std::holds_alternative<Just>(value)) {
            auto prev_consumed = consumed;
            auto result = next(std::move(*this));
            auto merged = merge_consumed(prev_consumed, result.consumed);
            return ParseResult(std::move(result.value), merged, result.rest);
        }
        return std::move(*this);
    }

    template<typename F>
    ParseResult or_else(F&& next) {
        if (std::holds_alternative<Nothing>(value)) {
            return next(std::move(*this));
        }
        return std::move(*this);
    }

    template<typename F>
    ParseResult on_fail(F&& next) {
        if (std::holds_alternative<Err>(value)) {
            return next(std::move(std::get<Err>(value).error));
        }
        return std::move(*this);
    }

    bool is_error() {
        return std::holds_alternative<Err>(value);
    }

    std::size_t size() {
        return consumed.size();
    }
};