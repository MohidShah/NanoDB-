CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Wpedantic -Isrc
BUILD    = build

# ── Collect all .cpp source files ─────────────────────────────────────────────
SRCS = src/Logger.cpp \
       src/memory/Page.cpp \
       src/memory/LRUCache.cpp \
       src/memory/BufferPool.cpp \
       src/schema/Field.cpp \
       src/schema/Row.cpp \
       src/schema/Schema.cpp \
       src/schema/Table.cpp \
       src/catalog/HashMap.cpp \
       src/catalog/SystemCatalog.cpp \
       src/parser/Tokenizer.cpp \
       src/parser/ShuntingYard.cpp \
       src/parser/ExprEvaluator.cpp \
       src/queue/PriorityQueue.cpp \
       src/index/AVLTree.cpp \
       src/optimizer/UnionFind.cpp \
       src/optimizer/Graph.cpp \
       src/optimizer/KruskalMST.cpp \
       src/engine/QueryExecutor.cpp

OBJS         = $(SRCS:src/%.cpp=$(BUILD)/%.o)
MAIN_OBJ     = $(BUILD)/main.o
RUNNER_OBJ   = $(BUILD)/test_runner_main.o

.PHONY: all clean

all: nanodb test_runner

# ── Main executable ────────────────────────────────────────────────────────────
nanodb: $(OBJS) $(MAIN_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

# ── Test runner executable ─────────────────────────────────────────────────────
test_runner: $(OBJS) $(RUNNER_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

# ── Compile shared sources ─────────────────────────────────────────────────────
$(BUILD)/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(MAIN_OBJ): src/main.cpp
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(RUNNER_OBJ): test_runner.cpp
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD) nanodb test_runner nanodb_execution.log
