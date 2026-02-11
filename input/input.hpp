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

    // Looks at the current character peek() is looking at.
    std::optional<unsigned char> peek();
    // stores peek(), then calls .next()
    std::optional<unsigned char> consume();
    // advances char_buff + 1. Will not advance past EOF
    void next();
    // unadvances char_buff. Will not advanced before 1. 
    void back();
    // if current char_buff is eof
    bool eof() const;

    std::size_t get_line_number() const;
    std::size_t get_col_number() const;

    std::expected<std::string, ErrorCode> get_line(std::size_t line_number) const;
    static std::expected<Input, ErrorCode> create(const std::string& filepath);

private:
    std::vector<unsigned char> buff;
    // Lookup table that returns where each line is.
    // For instance, if your file is "hello\nworld", 
    // then line_idxs would = { 0, 5 } because the next
    // line starts just after the '\n' (the 5th position).
    // Note that lines are 1 indexed so if you need to get
    // the a line manually you would call line_idxs[line - 1]
    std::vector<std::size_t> line_idxs = { 0 };
    std::size_t buff_idx = 0;
    std::size_t curr_line = 0;
    std::size_t curr_col = 0;

    // Helper functions for finding the current positions based on the buffer. 
    // For the most part youshould highly favor the public get_line_number() 
    // and get_col_number() as get_line_number_from_vec() is O(log n) time. 
    // get_col_number_from_vec() can be ok to call if you supply the current line number.
    // These could potentially be useful for a .seek() implementation or something
    // like that in the future.
    std::size_t get_line_number_from_vec();
    std::size_t get_col_number_from_vec();
    std::size_t get_col_number_from_vec(std::size_t curr_line);

    void read(std::istream& in);
    Input() = default;
};