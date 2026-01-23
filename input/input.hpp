#pragma once

#include <vector>
#include <string>
#include <istream>
#include <expected>
#include <optional>

class Input {

public:
    enum ErrorCode {
        FAILED_TO_READ_FILE,
        OUT_OF_BOUNDS
    };

    std::optional<unsigned char> peek();
    std::optional<unsigned char> consume();
    void next();
    void back();
    bool eof();

    std::size_t get_line_number();
    std::size_t get_col_number();

    std::expected<std::string, ErrorCode> get_line(std::size_t line_number);
    static std::expected<Input, ErrorCode> create(const std::string& filepath);

private:
    std::vector<unsigned char> buff;
    std::vector<std::size_t> line_idxs = { 0 };
    std::size_t buff_idx = 0;
    std::size_t curr_line = 0;
    std::size_t curr_col = 0;

    std::size_t get_line_number_from_vec();
    std::size_t get_col_number_from_vec();
    std::size_t get_col_number_from_vec(std::size_t curr_line);

    void read(std::istream& in);
    Input() = default;
};