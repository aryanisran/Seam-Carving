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
             $(SRC_DIR)/seam_carver.cpp \
             $(SRC_DIR)/benchmark.cpp

CORE_OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(CORE_SRCS))
MAIN_OBJ := $(BUILD_DIR)/main.o
TEST_OBJ := $(BUILD_DIR)/test_graph_cut.o
TEST_DP_OBJ := $(BUILD_DIR)/test_dp.o

TARGET := $(BIN_DIR)/seam_carve
TEST_TARGET := $(BIN_DIR)/test_graph_cut
TEST_DP_TARGET := $(BIN_DIR)/test_dp

.PHONY: all test clean directories

all: directories $(TARGET)

directories:
	@mkdir -p $(BUILD_DIR) $(BIN_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/test_graph_cut.o: $(TEST_DIR)/test_graph_cut.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/test_dp.o: $(TEST_DIR)/test_dp.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TARGET): $(CORE_OBJS) $(MAIN_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(TEST_TARGET): directories $(CORE_OBJS) $(TEST_OBJ)
	$(CXX) $(CXXFLAGS) $(CORE_OBJS) $(TEST_OBJ) -o $@ $(LDFLAGS)

$(TEST_DP_TARGET): directories $(CORE_OBJS) $(TEST_DP_OBJ)
	$(CXX) $(CXXFLAGS) $(CORE_OBJS) $(TEST_DP_OBJ) -o $@ $(LDFLAGS)

test: $(TEST_TARGET) $(TEST_DP_TARGET)
	./$(TEST_TARGET)
	./$(TEST_DP_TARGET)

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)
