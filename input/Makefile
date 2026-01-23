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
	@echo "Test passed"
	@rm main
	@rm tests/copy

same-position: $(DRIVER_DIR)/same-position.cpp $(SRC)
	@$(CXX) $(SRC) $< -o main $(CXXFLAGS)
	@./main
	bash -c 'diff <(tac tests/init) tests/final'
	@echo "Test passed"
	@rm main
	@rm tests/final
	@rm tests/init

%: $(DRIVER_DIR)/%.cpp $(SRC)
	@$(CXX) $(SRC) $< -o main $(CXXFLAGS)
	@./main
	@rm main

clean:
	@rm -f main
	@rm -f tests/copy
	@rm -f tests/final
	@rm -f tests/init