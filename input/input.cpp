#include "input.hpp"
#include <fstream>
#include <string>
#include <algorithm>

void Input::read(std::istream& in) {
    int c;
    int i = 0;

    while ((c = in.get()) != std::char_traits<char>::eof()) {
        i++;

        buff.push_back(c);

        if (c == '\n') {
            line_idxs.push_back(i);
        }

    }

}


int Input::peek() {
    if (eof()) {
        return ErrorCode::OUT_OF_BOUNDARY;
    }

    return buff[buff_idx];
}

void Input::next() {
    buff_idx++;
}

void Input::back() {
    buff_idx--;
}

int Input::consume() {
    int c = peek();
    next();
    return c;
}

bool Input::eof() {
    return buff_idx >= buff.size();
}

std::size_t Input::get_line() {
    auto it = std::lower_bound(line_idxs.begin(), line_idxs.end(), buff_idx);
    return std::distance(line_idxs.begin(), it);
}

std::size_t Input::get_col() {
    auto line_start = line_idxs[get_line() - 1];
    return (buff_idx - line_start);
}

Input::Input(std::istream& in) {
    read(in);
}

Input::Input(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);

    if (!file) {
        err = ErrorCode::FAILED_TO_READ_FILE;
        return;
    }

    read(file);
    file.close();
}

Input::~Input() {
    // Taken care of by RAII
}

