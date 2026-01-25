CXX = g++
CXXFLAGS = -std=c++23
SRC = input/input.cpp lexer.cpp error.cpp token.cpp main.cpp


all:
	$(CXX) $(SRC) -o main $(CXXFLAGS) -Wall -Wextra -Wpedantic -Wconversion

test:
	$(CXX) $(SRC) -o main $(CXXFLAGS) -Wall -Wextra -Wpedantic -Wconversion
	python3 test.py | colordiff