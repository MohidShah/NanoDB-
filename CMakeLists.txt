cmake_minimum_required(VERSION 3.15)
project(NanoDB CXX)

# ── Language standard ──────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Compiler warnings (help catch bugs early) ─────────────────────────────────
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ── Shared source files (all layers) ──────────────────────────────────────────
set(NANODB_SOURCES
    # Logging utility
    src/Logger.cpp

    # Layer 1 – Memory
    src/memory/Page.cpp
    src/memory/LRUCache.cpp
    src/memory/BufferPool.cpp

    # Layer 2 – Schema
    src/schema/Field.cpp
    src/schema/Row.cpp
    src/schema/Schema.cpp
    src/schema/Table.cpp

    # Layer 3 – System Catalog
    src/catalog/HashMap.cpp
    src/catalog/SystemCatalog.cpp

    # Layer 4 – Parser
    src/parser/Tokenizer.cpp
    src/parser/ShuntingYard.cpp
    src/parser/ExprEvaluator.cpp

    # Layer 5 – Priority Queue
    src/queue/PriorityQueue.cpp

    # Layer 6 – AVL Index
    src/index/AVLTree.cpp

    # Layer 7 – Optimizer
    src/optimizer/UnionFind.cpp
    src/optimizer/Graph.cpp
    src/optimizer/KruskalMST.cpp

    # Layer 8 – Engine
    src/engine/QueryExecutor.cpp
)

# ── Main executable ────────────────────────────────────────────────────────────
add_executable(nanodb
    src/main.cpp
    ${NANODB_SOURCES}
)
target_include_directories(nanodb PRIVATE src)

# ── Automated test runner ──────────────────────────────────────────────────────
add_executable(test_runner
    test_runner.cpp
    ${NANODB_SOURCES}
)
target_include_directories(test_runner PRIVATE src)
