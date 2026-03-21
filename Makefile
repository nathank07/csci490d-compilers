CXX = g++
CXXFLAGS = -std=c++23 -Wall -Wextra -Wpedantic -Wconversion -MMD -MP -D_GLIBCXX_ASSERTIONS -g -fsanitize=undefined -Wshadow -Wunused # -fsanitize=address

COMMON_SRC = input/input.cpp lexer/lexer.cpp lexer/error.cpp lexer/token.cpp ast/ast.cpp

NCC_SRC = $(COMMON_SRC) main.cpp
NCC_OBJ = $(NCC_SRC:.cpp=.o)

MCC_SRC = $(COMMON_SRC) mcc.cpp
MCC_OBJ = $(MCC_SRC:.cpp=.o)

DEP = $(NCC_OBJ:.o=.d) $(MCC_OBJ:.o=.d)

all: ncc

ncc: $(NCC_OBJ)
	$(CXX) $(NCC_OBJ) -o ncc $(CXXFLAGS)

mcc: $(MCC_OBJ)
	$(CXX) $(MCC_OBJ) -o mcc $(CXXFLAGS)

%.o: %.cpp
	$(CXX) -c $< -o $@ $(CXXFLAGS)

-include $(DEP)

test: ncc
	python3 test.py | colordiff

clean:
	rm -f $(NCC_OBJ) $(MCC_OBJ) $(DEP) ncc mcc

.PHONY: all test clean
