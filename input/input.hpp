#pragma once

#include <vector>
#include <string>
#include <istream>

class Input {

public:
    enum ErrorCode {
        OK = 0,
        FAILED_TO_READ_FILE = -1,
        OUT_OF_BOUNDS = -2
    };

    Input(std::istream& in);
    Input(const std::string& filepath);
    ~Input();
    int peek();
    void next();
    int consume();
    void back();
    bool eof();

    std::size_t get_line_number();
    std::size_t get_col_number();
    std::string get_line(std::size_t line_number);

private:

    std::vector<char> buff;
    std::vector<std::size_t> line_idxs = { 0 };
    std::size_t buff_idx = 0;
    ErrorCode err = ErrorCode::OK;

    void read(std::istream& in);
    
};