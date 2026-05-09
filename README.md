# NanoDB — Mini Relational Database Engine

A from-scratch relational database engine written in C++ (no STL containers).

NanoDB is a fully functional database architecture built to demonstrate deep understanding of core data structures, manual memory management, and file persistence. It implements its own custom buffer pool, system catalog, B-tree-style AVL indexes, parsing engine, and an MST-based query optimizer. The engine is capable of processing TPC-H style benchmark workloads (upwards of 100,000 synthesized records) entirely through manual, low-level heap allocations while adhering strictly to zero-STL constraints.
## GitHub Repository
> **Link:** [https://github.com/MohidShah/NanoDB-](https://github.com/MohidShah/NanoDB-)

---

## Architecture

| Layer | Data Structure | Complexity |
|---|---|---|
| Buffer Pool | Raw `char*` pages + DLL LRU | Evict O(1) |
| Schema Types | Abstract `Field` + derived classes | — |
| System Catalog | FNV-1a Hash Map (chaining) | Lookup O(1) avg |
| Query Parser | Custom Stack + Shunting-Yard | O(N tokens) |
| Priority Queue | Binary Max-Heap | Enqueue/Dequeue O(log N) |
| AVL Index | Self-balancing BST | Search O(log N) |
| Join Optimizer | Adjacency matrix + Kruskal MST | O(E log E) |

---

## How to Build

### Option A — CMake (recommended)
```bash
cmake -B build -S .
cmake --build build
```

### Option B — GNU Make
```bash
make all
```

Both produce two executables:
- `nanodb` — interactive engine
- `test_runner` — automated workload runner

---

## How to Run the Test Suite

```bash
# Generate TPC-H data first (requires Python 3)
python data/generate_tpch.py

# Run the automated test runner (reads queries.txt, writes nanodb_execution.log)
./test_runner          # Linux/Mac
.\test_runner.exe      # Windows
```

The log file `nanodb_execution.log` will contain all `[LOG]` events including:
- LRU cache evictions
- Infix → Postfix conversions
- MST join plans
- AVL rotation events
- Priority queue dispatches

---

## Dataset

TPC-H benchmark data, scale factor ~0.1:
- `customer` — 20,000 rows
- `orders` — 30,000 rows
- `lineitem` — 50,000 rows

Raw `.tbl` files are **not** committed to Git (see `.gitignore`). Regenerate with:
```bash
python data/generate_tpch.py
```

---

## Memory Safety

Verify no leaks with:
```bash
valgrind --leak-check=full --show-leak-kinds=all ./nanodb
```

---

## Project Structure

```
NanoDB/
├── src/
│   ├── Logger.h / Logger.cpp
│   ├── memory/        # Buffer Pool, Page, LRU Cache
│   ├── schema/        # Field, Row, Schema, Table
│   ├── catalog/       # HashMap, SystemCatalog
│   ├── parser/        # Tokenizer, Stack, ShuntingYard, ExprEvaluator
│   ├── queue/         # QueryJob, PriorityQueue
│   ├── index/         # AVLNode, AVLTree
│   ├── optimizer/     # UnionFind, Graph, KruskalMST
│   ├── engine/        # QueryExecutor
│   └── main.cpp
├── data/
│   └── generate_tpch.py
├── queries.txt
├── test_runner.cpp
├── CMakeLists.txt
└── Makefile
```
