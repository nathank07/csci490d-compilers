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


    std::span<const Token> grow_consumed() const {
        const Token* start = consumed.empty() ? rest.data() : consumed.data();
        return std::span<const Token>(start, consumed.size() + 1);
    }

    ParseResult want_tok(const TokenType wanted) {
        if (std::holds_alternative<Err>(value)) return std::move(*this);
        if (rest.front().type == wanted) {
            auto new_consumed = grow_consumed();
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
            auto new_consumed = grow_consumed();
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
            auto new_consumed = grow_consumed();
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
            auto new_consumed = grow_consumed();
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
            if (!prev_consumed.empty()) {
                auto merged = std::span<const Token>(
                    prev_consumed.data(),
                    prev_consumed.size() + result.consumed.size());
                return ParseResult{std::move(result.value), merged, result.rest};
            }
            return result;
        }
        return std::move(*this);
    }

    template<typename F>
    ParseResult or_else(F&& next) {
        if (std::holds_alternative<Nothing>(value)) {
            return next(*this);
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