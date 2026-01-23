CXX = g++
CXXFLAGS = -std=c++23
SRC = input.cpp
DRIVER_DIR = drivers

all:
	@echo "Please specify a test (identical-io, identical-lines, lines-and-columns, out-of-bounds, same-position)"

identical-io: $(DRIVER_DIR)/identical-io.cpp $(SRC)
	@$(CXX) $(SRC) $< -o main $(CXXFLAGS)
	@./main
	diff tests/ChessBoard.tsx tests/copy
	@echo "If the program works correctly, diff should return nothing"
	@rm main
	@rm tests/copy

%: $(DRIVER_DIR)/%.cpp $(SRC)
	@$(CXX) $(SRC) $< -o main $(CXXFLAGS)
	@./main
	@rm main

clean:
	@rm -f main