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

    std::span<const Token> undo_consumed() const {
        if (consumed.empty()) return rest;
        return std::span<const Token>(consumed.data(), consumed.size() + rest.size());
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
            auto acc_consumed = acc.consumed;
            auto acc_rest = acc.rest;
            return f(std::move(acc), ParseResult::nothing(acc_rest))
                .and_then([&](auto&& next) {
                    return take_while_just_left(std::move(next), std::forward<F>(f));
                })
                .or_else([&](auto&&) {
                    return ParseResult(Just{std::move(*acc)}, acc_consumed, acc_rest);
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
                        return combine(std::move(lhs), std::move(rhs));
                    })
                    .or_else([&](auto&& stopped) {
                        return stopped.create_expr(std::move(*lhs));
                    });
            });
    }

    template <typename P, typename S, typename F>
    ParseResult then_want_right_sep(P&& parse_next, S&& sep, F&& combine) {
        if (std::holds_alternative<Just>(value)) {
            auto prev_consumed = consumed;
            auto prev_rest = rest;
            auto result = want_right_sep(std::forward<P>(parse_next), std::forward<S>(sep), std::forward<F>(combine));
            if (std::holds_alternative<Just>(result.value))
                return ParseResult(std::move(result.value), merge_consumed(prev_consumed, result.consumed), result.rest);
            // Nothing found — fall back to pre-call state
            return ParseResult(Just{T{}}, prev_consumed, prev_rest);
        }
        return std::move(*this);
    }

    /* ========================================================================

        want_tok, or_want_tok, and then_want_tok are helpers for creating ASTs
        by checking the current state of the ParseResult and acting accordingly.

        The general patterns goes like so -

          * Start from ::nothing(span), use want_tok/1, then chain as needed. 

          * All *_tok/2 functions will pass one ParseResult arg lambda (with the
            exception of the previously aforementioned .then_*_tok/2). This allows
            you to store the result in Just.

          * If you need to access the functions, the previously aforementioned 
            .then_*_tok/2 works - or you can always bind it with .and_then.

          * or_want_tok/1 and or_want_tok/2 are used to check if the result
            returned nothing. For instance you can check if the first token like so:
                
                result
                    .want_tok(A)
                    .or_want_tok(B). 
                
            This will return ParseResult advanced once for A and B, but won't for C.

          * then_want_tok/1 returns nothing on mismatch - this is helpful because it
            allows you to fail early without propagating an unnecessary err. This is 
            consistent with other *want_tok/1 behavior - so if you need the value, you
            need to use /2.

          * .then_want_tok is used after you use want_tok - this checks if *want_tok 
            found anything, then acts on it. Since then_want_tok/2 implies there's an
            expression, the lambda provides you with a the current value, and the
            rest of the ParseResult, so you can act on it.
            
          * .*_expect_tok does the same thing as *_want_tok, but if they don't match,
            it propagates an error.
    
       ======================================================================== */

    ParseResult want_tok(const TokenType wanted) {
        if (std::holds_alternative<Err>(value)) return std::move(*this);
        if (rest.front().type == wanted) {
            auto new_consumed = merge_consumed(consumed, rest.subspan(0, 1));
            return ParseResult{Just{T{}},
                new_consumed,
                rest.subspan(1)};
        }

        return ParseResult{Nothing{}, {}, rest};
    }

    template <typename F>
    ParseResult want_tok(const TokenType wanted, F&& make_this) {
        if (std::holds_alternative<Err>(value)) return std::move(*this);
        if (rest.front().type == wanted) {
            auto new_consumed = merge_consumed(consumed, rest.subspan(0, 1));
            auto continuation = ParseResult{Just{T{}}, new_consumed, rest.subspan(1)};
            auto result = make_this(std::move(continuation));
            return ParseResult(std::move(result.value), merge_consumed(new_consumed, result.consumed), result.rest);
        }

        return ParseResult{Nothing{}, {}, rest};
    }

    ParseResult then_want_tok(const TokenType wanted) {
        if(std::holds_alternative<Just>(value)) {
            if (rest.front().type == wanted) {
                auto new_consumed = merge_consumed(consumed, rest.subspan(0, 1));
                return ParseResult{Just{std::move(std::get<Just>(value).value)},
                    new_consumed, rest.subspan(1)};
            }
            return ParseResult{Nothing{}, {}, undo_consumed()};
        }

        return std::move(*this);
    }

    template <typename F>
    ParseResult then_want_tok(const TokenType expected, F&& make_this) {
        if(std::holds_alternative<Just>(value)) {
            if (rest.front().type == expected) {
                auto new_consumed = merge_consumed(consumed, rest.subspan(0, 1));
                auto continuation = ParseResult{Just{T{}}, new_consumed, rest.subspan(1)};
                auto result = make_this(std::move(std::get<Just>(value).value), std::move(continuation));
                return ParseResult(std::move(result.value), merge_consumed(new_consumed, result.consumed), result.rest);
            }
            return ParseResult{Nothing{}, {}, undo_consumed()};
        }

        return std::move(*this);
    }
    
    ParseResult or_want_tok(const TokenType wanted) {
        if(std::holds_alternative<Nothing>(value)) 
            return want_tok(wanted);
            
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
            auto new_consumed = merge_consumed(consumed, rest.subspan(0, 1));
            return ParseResult{Just{T{}},
                new_consumed,
                rest.subspan(1)};
        }

        return std::move(ParseResult::error(AstError::bad_symbol(rest.front())));
    }

    template <typename F>
    ParseResult expect_tok(const TokenType expected, F&& make_this) {
        if (std::holds_alternative<Err>(value)) return std::move(*this);
        if (rest.front().type == expected) {
            auto new_consumed = merge_consumed(consumed, rest.subspan(0, 1));
            auto continuation = ParseResult{Just{T{}}, new_consumed, rest.subspan(1)};
            auto result = make_this(std::move(continuation));
            return ParseResult(std::move(result.value), merge_consumed(new_consumed, result.consumed), result.rest);
        }

        return std::move(ParseResult::error(AstError::bad_symbol(rest.front())));
    }

    ParseResult then_expect_tok(const TokenType expected) {
        if(std::holds_alternative<Just>(value)) {
            if (rest.front().type == expected) {
                auto new_consumed = merge_consumed(consumed, rest.subspan(0, 1));
                return ParseResult{Just{std::move(std::get<Just>(value).value)},
                    new_consumed, rest.subspan(1)};
            }
            return std::move(ParseResult::error(AstError::bad_symbol(rest.front())));
        }

        return std::move(*this);
    }

    template <typename F>
    ParseResult then_expect_tok(const TokenType expected, F&& make_this) {
        if(std::holds_alternative<Just>(value)) {
            if (rest.front().type == expected) {
                auto new_consumed = merge_consumed(consumed, rest.subspan(0, 1));
                auto continuation = ParseResult{Just{T{}}, new_consumed, rest.subspan(1)};
                auto result = make_this(std::move(std::get<Just>(value).value), std::move(continuation));
                return ParseResult(std::move(result.value), merge_consumed(new_consumed, result.consumed), result.rest);
            }
            return std::move(ParseResult::error(AstError::bad_symbol(rest.front())));
        }

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
    ParseResult and_then(F&& next) requires std::is_invocable_v<F, ParseResult&&> {
        if (std::holds_alternative<Just>(value)) {
            auto prev_consumed = consumed;
            auto result = next(std::move(*this));
            auto merged = merge_consumed(prev_consumed, result.consumed);
            return ParseResult(std::move(result.value), merged, result.rest);
        }
        return std::move(*this);
    }

    template <typename F>
    ParseResult and_then(F&& next) requires std::is_invocable_v<F, T&&, ParseResult&&> {
        if (std::holds_alternative<Just>(value)) {
            auto prev_consumed = consumed;
            auto continuation = ParseResult{Just{T{}}, consumed, rest};
            auto result = next(std::move(std::get<Just>(value).value), std::move(continuation));
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