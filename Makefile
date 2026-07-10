CXX := g++
CXXFLAGS_COMMON := -std=c++20 -Wall -Wextra -pthread
CXXFLAGS_RELEASE := $(CXXFLAGS_COMMON) -O2
CXXFLAGS_TSAN := $(CXXFLAGS_COMMON) -O1 -g -fsanitize=thread

BIN_DIR := bin

.PHONY: all stress stress-tsan bench false-sharing-demo clean

all: stress bench false-sharing-demo

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

stress: $(BIN_DIR)
	$(CXX) $(CXXFLAGS_RELEASE) test/stress_test.cpp -o $(BIN_DIR)/stress_test

# ThreadSanitizer build: catches races the plain stress test can miss by luck.
stress-tsan: $(BIN_DIR)
	$(CXX) $(CXXFLAGS_TSAN) test/stress_test.cpp -o $(BIN_DIR)/stress_test_tsan

bench: $(BIN_DIR)
	$(CXX) $(CXXFLAGS_RELEASE) bench/benchmark.cpp -o $(BIN_DIR)/benchmark

false-sharing-demo: $(BIN_DIR)
	$(CXX) $(CXXFLAGS_RELEASE) bench/false_sharing_demo.cpp -o $(BIN_DIR)/false_sharing_demo

clean:
	rm -rf $(BIN_DIR)
