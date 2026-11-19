CXX := g++
CXXFLAGS := -std=c++17 -O3 -Wall -Wextra -Wpedantic -Iinclude
LDFLAGS := -lm

SRC_DIR := src
TEST_DIR := tests
BUILD_DIR := build
BIN_DIR := bin

CORE_SRCS := $(SRC_DIR)/image.cpp \
             $(SRC_DIR)/energy.cpp \
             $(SRC_DIR)/graph.cpp \
             $(SRC_DIR)/maxflow.cpp \
             $(SRC_DIR)/graph_cut_seam.cpp \
             $(SRC_DIR)/dp_seam.cpp \
             $(SRC_DIR)/greedy_seam.cpp \
             $(SRC_DIR)/seam_carver.cpp \
             $(SRC_DIR)/benchmark.cpp

CORE_OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(CORE_SRCS))
MAIN_OBJ := $(BUILD_DIR)/main.o
TEST_OBJ := $(BUILD_DIR)/test_graph_cut.o
TEST_DP_OBJ := $(BUILD_DIR)/test_dp.o
TEST_GREEDY_OBJ := $(BUILD_DIR)/test_greedy.o
COMPARE_OBJ := $(BUILD_DIR)/compare_greedy_dp.o
BENCHMARK_OBJ := $(BUILD_DIR)/benchmark_algorithms.o

TARGET := $(BIN_DIR)/seam_carve
TEST_TARGET := $(BIN_DIR)/test_graph_cut
TEST_DP_TARGET := $(BIN_DIR)/test_dp
TEST_GREEDY_TARGET := $(BIN_DIR)/test_greedy
COMPARE_TARGET := $(BIN_DIR)/compare_greedy_dp
BENCHMARK_TARGET := $(BIN_DIR)/benchmark_algorithms

.PHONY: all test compare benchmark clean directories

all: directories $(TARGET)

directories:
	@mkdir -p $(BUILD_DIR) $(BIN_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/test_graph_cut.o: $(TEST_DIR)/test_graph_cut.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/test_dp.o: $(TEST_DIR)/test_dp.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/test_greedy.o: $(TEST_DIR)/test_greedy.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/compare_greedy_dp.o: $(TEST_DIR)/compare_greedy_dp.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/benchmark_algorithms.o: $(TEST_DIR)/benchmark_algorithms.cpp
	$(CXX) $(CXXFLAGS) -fopenmp -c $< -o $@

$(TARGET): $(CORE_OBJS) $(MAIN_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(TEST_TARGET): directories $(CORE_OBJS) $(TEST_OBJ)
	$(CXX) $(CXXFLAGS) $(CORE_OBJS) $(TEST_OBJ) -o $@ $(LDFLAGS)

$(TEST_DP_TARGET): directories $(CORE_OBJS) $(TEST_DP_OBJ)
	$(CXX) $(CXXFLAGS) $(CORE_OBJS) $(TEST_DP_OBJ) -o $@ $(LDFLAGS)

$(TEST_GREEDY_TARGET): directories $(CORE_OBJS) $(TEST_GREEDY_OBJ)
	$(CXX) $(CXXFLAGS) $(CORE_OBJS) $(TEST_GREEDY_OBJ) -o $@ $(LDFLAGS)

$(COMPARE_TARGET): directories $(CORE_OBJS) $(COMPARE_OBJ)
	$(CXX) $(CXXFLAGS) $(CORE_OBJS) $(COMPARE_OBJ) -o $@ $(LDFLAGS)

$(BENCHMARK_TARGET): directories $(CORE_OBJS) $(BENCHMARK_OBJ)
	$(CXX) $(CXXFLAGS) $(CORE_OBJS) $(BENCHMARK_OBJ) -o $@ $(LDFLAGS) -fopenmp

test: $(TEST_TARGET) $(TEST_DP_TARGET) $(TEST_GREEDY_TARGET)
	./$(TEST_TARGET)
	./$(TEST_DP_TARGET)
	./$(TEST_GREEDY_TARGET)

compare: $(COMPARE_TARGET)
	@mkdir -p results/greedy_vs_dp
	./$(COMPARE_TARGET) --out results/greedy_vs_dp
	./$(COMPARE_TARGET) --image samples/image.png --out results/greedy_vs_dp

benchmark: $(BENCHMARK_TARGET)
	@mkdir -p results/benchmark
	./$(BENCHMARK_TARGET) --out results/benchmark

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)
