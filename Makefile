CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra
LDFLAGS := -pthread

BUILD_DIR := build

SRC := ram_file_system.cpp
TEST_SRC := test_ram_file_system.cpp
CONCURRENCY_TEST_SRC := test_ram_file_system_concurrency.cpp

TEST_BIN := $(BUILD_DIR)/test_ram_file_system
CONCURRENCY_TEST_BIN := $(BUILD_DIR)/test_ram_file_system_concurrency
TSAN_TEST_BIN := $(BUILD_DIR)/test_ram_file_system_concurrency_tsan

.PHONY: all test tsan clean

all: $(TEST_BIN) $(CONCURRENCY_TEST_BIN)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TEST_BIN): $(SRC) $(TEST_SRC) ram_file_system.hpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(SRC) $(TEST_SRC) -o $@

$(CONCURRENCY_TEST_BIN): $(SRC) $(CONCURRENCY_TEST_SRC) ram_file_system.hpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(SRC) $(CONCURRENCY_TEST_SRC) -o $@

$(TSAN_TEST_BIN): $(SRC) $(CONCURRENCY_TEST_SRC) ram_file_system.hpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -fsanitize=thread -g -O1 $(SRC) $(CONCURRENCY_TEST_SRC) -o $@

test: $(TEST_BIN) $(CONCURRENCY_TEST_BIN)
	./$(TEST_BIN)
	./$(CONCURRENCY_TEST_BIN)

tsan: $(TSAN_TEST_BIN)
	./$(TSAN_TEST_BIN)

clean:
	rm -rf $(BUILD_DIR)
