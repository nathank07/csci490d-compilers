#include "input.hpp"
#include <fstream>
#include <string>
#include <algorithm>

void Input::read(std::istream& in) {
    int c;
    int i = 0;
    while ((c = in.get()) != std::char_traits<char>::eof()) {
        i++;

        if (c == '\n') {
            line_idxs.push_back(i);
        }

        buff.push_back(c);
        
    }
}

std::optional<unsigned char> Input::peek() {
    if (eof()) {
        return std::nullopt;
    }

    return buff[buff_idx];
}

void Input::next() {
    if (!eof()) buff_idx++;
}

void Input::back() {
    if (buff_idx > 0) buff_idx--;
}

std::optional<unsigned char> Input::consume() {
    auto c = peek();
    next();
    return c;
}

bool Input::eof() {
    return buff_idx >= buff.size();
}

std::size_t Input::get_line_number() {
    if (buff_idx == 0) return 1;

    auto it = std::lower_bound(line_idxs.begin(), line_idxs.end(), buff_idx);
    return std::distance(line_idxs.begin(), it);
}

std::size_t Input::get_col_number() {
    auto line_start = line_idxs[get_line_number() - 1];
    return (buff_idx - line_start) + 1;
}

std::expected<std::string, Input::ErrorCode> Input::get_line(std::size_t line_number) {

    if (line_number > line_idxs.size()) {
        return std::unexpected(Input::ErrorCode::OUT_OF_BOUNDS);
    }

    auto start = buff.begin() + line_idxs[line_number - 1];
    
    auto end = line_number != line_idxs.size() ?
                 buff.begin() + line_idxs[line_number]
               : buff.end();

    return std::string(start, end);
}

std::expected<Input, Input::ErrorCode> Input::create(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);

    if (!file) {
        return std::unexpected(Input::ErrorCode::FAILED_TO_READ_FILE);
    }

    auto self = Input();
    self.read(file);

    return self;
}




