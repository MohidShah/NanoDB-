# NanoDB — Mini Relational Database Engine

**Semester Project: Systems Architecture & Query Optimizer**

NanoDB is a fully functional relational database engine written entirely from scratch in C++. It was built under strict **Zero-STL (Standard Template Library) constraints** to demonstrate a deep, low-level understanding of core data structures, manual memory management (`new`/`delete`), and persistent disk I/O operations.

Rather than relying on `std::vector` or `std::map`, this engine features:
- A bespoke **Doubly-Linked-List LRU Cache** for the Buffer Pool to prevent Out-Of-Memory crashes.
- A custom **FNV-1a Hash Map** for the System Catalog.
- **AVL Self-Balancing Trees** for $O(\log N)$ index lookups.
- A **Shunting-Yard Stack Parser** for converting SQL expressions to Postfix evaluation trees.
- A **Graph Adjacency Matrix with Kruskal's MST** for multi-table join optimization.

The engine is capable of processing industry-standard **TPC-H benchmark workloads** (upwards of 100,000 synthesized records), serializing abstract polymorphic objects into fixed-width binary pages.

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

## How to Run & What to Expect

### Step 1: Generate the TPC-H Dataset
Before running the engine, you must generate the 100,000-record dataset.
```bash
python data/generate_tpch.py
```
*Expected Output:* The script will print its progress and generate three files (`customer.tbl`, `orders.tbl`, `lineitem.tbl`) in the `data/` directory.

### Step 2: Run the Main Engine & Workload
The main executable boots the database and automatically processes the 50 SQL-like commands stored in the `queries.txt` Workload File.
```bash
# Windows
.\nanodb.exe
```
*Expected Output:* The console will display the engine booting up and processing queries. Crucially, it will generate a highly detailed `nanodb_execution.log` file in your root folder. If you open this log, you will see real-time backend events such as:
- `[LOG] Page 42 evicted via LRU, written to disk.`
- `[LOG] Infix "c_acctbal > 5000" converted to Postfix "c_acctbal 5000 >"`
- `[LOG] Multi-table join routed via MST: customer -> orders -> lineitem`

### Step 3: Run the Automated Evaluation Test Suite
To verify the specific architecture constraints, run the automated test runner:
```bash
# Windows
.\test_runner.exe
```
*Expected Output:* The test runner will sequentially execute and print the results of 7 distinct test cases directly to the console:
1. **[Test A] Parser & Evaluator:** Prints a Postfix tree generation.
2. **[Test B] Index Optimizer:** Compares the execution time of a Sequential Array Scan vs. an AVL Tree lookup on 100k rows (showing a massive speedup).
3. **[Test C] Join Optimizer:** Prints the Minimal Spanning Tree (MST) path chosen by Kruskal's algorithm.
4. **[Test D] Memory Stress Test:** Artificially restricts the Buffer Pool to 5 pages and prints out the exact number of LRU cache evictions triggered to prevent a crash.
5. **[Test E] Priority Queue Concurrency:** Proves that an Admin `UPDATE` query bypasses background `SELECT` queries in the custom Max-Heap.
6. **[Test F] Deep Expression Tree:** Evaluates a mathematically complex, nested query without crashing.
7. **[Test G] Durability & Persistence:** Proves that data pages and catalog metadata successfully serialized to `.dbf` and `.meta` files, surviving a simulated system shutdown.

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
