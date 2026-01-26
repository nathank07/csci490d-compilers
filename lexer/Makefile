CXX = g++
CXXFLAGS = -std=c++23 -Wall -Wextra -Wpedantic -Wconversion -MMD -MP -D_GLIBCXX_ASSERTIONS -g -fsanitize=undefined -Wshadow -Wunused -fsanitize=address
SRC = input/input.cpp lexer.cpp error.cpp token.cpp main.cpp
OBJ = $(SRC:.cpp=.o)
DEP = $(OBJ:.o=.d)
TARGET = main

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(OBJ) -o $(TARGET) $(CXXFLAGS)

%.o: %.cpp
	$(CXX) -c $< -o $@ $(CXXFLAGS)

-include $(DEP)

test: $(TARGET)
	python3 test.py | colordiff

clean:
	rm -f $(OBJ) $(DEP) $(TARGET)

.PHONY: all test clean
