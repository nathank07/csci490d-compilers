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
    if (err) {
        return err;
    }

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

std::size_t Input::get_line_number() {
    auto it = std::lower_bound(line_idxs.begin(), line_idxs.end(), buff_idx);
    return std::distance(line_idxs.begin(), it);
}

std::size_t Input::get_col_number() {
    auto line_start = line_idxs[get_line_number() - 1];
    return (buff_idx - line_start);
}

std::string Input::get_line(std::size_t line_number) {

    if (line_number > line_idxs.size()) {
        return "";
    }

    auto start = buff.begin() + line_idxs[line_number - 1];
    
    auto end = line_number != line_idxs.size() ?
                 buff.begin() + line_idxs[line_number]
               : buff.end();

    return std::string(start, end);
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

