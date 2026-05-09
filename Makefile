CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Isrc
BUILD    = build_out

# ── All shared .cpp files ──────────────────────────────────────────────────────
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

.PHONY: all clean data

all: $(BUILD) nanodb.exe test_runner.exe

$(BUILD):
	if not exist $(BUILD) mkdir $(BUILD)

# ── Main executable ────────────────────────────────────────────────────────────
nanodb.exe: $(SRCS) src/main.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^

# ── Test runner executable ─────────────────────────────────────────────────────
test_runner.exe: $(SRCS) test_runner.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^

# ── Generate TPC-H data ────────────────────────────────────────────────────────
data:
	python data/generate_tpch.py

clean:
	if exist nanodb.exe del nanodb.exe
	if exist test_runner.exe del test_runner.exe
	if exist nanodb_execution.log del nanodb_execution.log
	if exist nanodb_test.log del nanodb_test.log
	if exist nanodb.catalog del nanodb.catalog
