#pragma once
#include <variant>
#include <optional>

template<typename T, typename E>
struct ParseResult {
    struct Just { T value; };
    struct Nothing {};
    struct Err { E error; };

    std::variant<Just, Nothing, Err> value;

    ParseResult(Just j) : value(std::move(j)) {}
    ParseResult(Nothing n) : value(n) {}
    ParseResult(Err e) : value(std::move(e)) {}

    
    static ParseResult just(T val) { return Just{std::move(val)}; }
    static ParseResult nothing() { return Nothing{}; }
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

    template <typename F>
    ParseResult and_then(F&& next) {
        if (std::holds_alternative<Just>(value)) {
            return next(std::move(std::get<Just>(value).value));
        }
        return std::move(*this); 
    }

    template <typename F>
    ParseResult transform(F&& f) {
        if (std::holds_alternative<Just>(value)) {
            auto current_val = std::move(std::get<Just>(value).value);
            auto new_val = f(std::move(current_val));
            return ParseResult(Just{std::move(new_val)});
        }
        return std::move(*this);
    }

    template<typename F>
    ParseResult or_else(F&& next) {
        if (std::holds_alternative<Nothing>(value)) {
            return next();
        }
        return std::move(*this);
    }

    bool is_error() {
        return std::holds_alternative<Err>(value);
    }

    std::optional<std::size_t> get_expr_width() {
        if (std::holds_alternative<Just>(value)) {
            return std::get<Just>(value).value->token_span.size();
        }

        return std::nullopt;
    }
};