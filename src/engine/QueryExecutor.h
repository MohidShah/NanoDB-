/*
 * QueryExecutor.h
 * ---------------
 * The QueryExecutor is the heart of NanoDB's runtime engine.
 *
 * It sits between the priority queue (scheduling) and the storage/index
 * layers (execution), connecting every component built.
 *
 * ── Supported SQL commands ───────────────────────────────────────────────────
 *   CREATE TABLE <name> (<col> <type>, ...)
 *   DROP   TABLE <name>
 *   LIST   TABLES
 *   INSERT INTO <table> VALUES (v1, v2, ...)
 *   SELECT * FROM <table> [WHERE <expr>]
 *   SELECT <cols> FROM <table> [WHERE <expr>]
 *   UPDATE <table> SET <col>=<val> [WHERE <expr>]   ← admin priority bypass
 *   DELETE FROM <table> WHERE <expr>
 *   SELECT * FROM t1, t2, t3 ON t1.c1=t2.c2 AND t2.c3=t3.c4  ← JOIN
 *
 * ── Admin bypass (Test Case E) ───────────────────────────────────────────────
 *   Any UPDATE statement is submitted with PRIORITY_ADMIN (=10).
 *   All other statements use PRIORITY_USER (=1).
 *   The PriorityQueue's binary max-heap ensures admin tasks run first.
 *
 * ── Index acceleration (Test Case B) ─────────────────────────────────────────
 *   One AVLTree per table (keyed on the first INT column).
 *   SELECT … WHERE <int-col> = <literal>  uses the AVL path.
 *   All other SELECTs fall back to a sequential O(N) Table::scan().
 *
 * ── Join optimizer (Test Case C) ─────────────────────────────────────────────
 *   Multi-table queries go through Graph + KruskalMST to determine join order.
 *   The executor then performs a nested-loop join in that order.
 *
 */
#ifndef QUERY_EXECUTOR_H
#define QUERY_EXECUTOR_H

#include "../Constants.h"
#include "../Logger.h"
#include "../catalog/SystemCatalog.h"
#include "../index/AVLTree.h"
#include "../memory/BufferPool.h"
#include "../optimizer/Graph.h"
#include "../optimizer/KruskalMST.h"
#include "../parser/ExprEvaluator.h"
#include "../parser/ShuntingYard.h"
#include "../parser/Tokenizer.h"
#include "../queue/PriorityQueue.h"
#include "../schema/Row.h"
#include "../schema/Table.h"
#include <stdio.h>  // printf, snprintf
#include <string.h> // strncpy, strcmp, strchr

// Maximum number of tables the executor tracks AVL indexes for.
static const int MAX_INDEXED_TABLES = 16;

// ── AVL index registry entry ─────────────────────────────────────────────────
struct IndexEntry {
  char tableName[MAX_TABLE_NAME];
  int colIndex;  // which column is indexed (first INT col)
  AVLTree *tree; // heap-allocated, owned by QueryExecutor

  IndexEntry() : colIndex(-1), tree(0) { tableName[0] = '\0'; }
};

// ── Context passed through Table::scan callbacks
// ──────────────────────────────
struct WhereCtx {
  const Token *postfix;
  int tokenCount;
  const Schema *schema;
  ExprEvaluator *evaluator;
  int matchCount;
  bool printRows;
};

// ── Context for UPDATE callbacks
// ──────────────────────────────────────────────
struct UpdateCtx {
  const Token *wherePostfix;
  int whereCount;
  const Schema *schema;
  ExprEvaluator *evaluator;
  Table *table;
  int setColIdx;
  char setVal[256];
  int updatedCount;
};

// ── Context for DELETE callbacks
// ──────────────────────────────────────────────
struct DeleteCtx {
  const Token *wherePostfix;
  int whereCount;
  const Schema *schema;
  ExprEvaluator *evaluator;
  int deletedCount;
};

// ── AVL index build context
// ───────────────────────────────────────────────────
struct IndexBuildCtx {
  AVLTree *tree;
  int colIdx;
  int pageId;
  int slotId;
};

// ── QueryExecutor
// ─────────────────────────────────────────────────────────────
class QueryExecutor {
public:
  /**
   * Constructor.
   * @param pool     Shared buffer pool — NOT owned by executor.
   * @param catalog  System catalog — NOT owned by executor.
   */
  QueryExecutor(BufferPool *pool, SystemCatalog *catalog);

  /** Destructor — frees AVL tree indexes. */
  ~QueryExecutor();

  /**
   * submit() — Tokenise an SQL string, assign priority, and enqueue it.
   * UPDATE statements are given PRIORITY_ADMIN; all others PRIORITY_USER.
   */
  void submit(const char *sql);

  /**
   * runAll() — Drain the priority queue and execute every pending statement.
   * Statements execute in priority order (highest first).
   */
  void runAll();

  /**
   * execute() — Execute one SQL string immediately (bypasses the queue).
   * Useful for direct calls in test_runner.
   */
  void execute(const char *sql);

  /** How many statements are still waiting in the queue? */
  int pendingCount() const { return queue.size(); }

private:
  BufferPool *pool;
  SystemCatalog *catalog;
  PriorityQueue queue;

  // ── AVL indexes ───────────────────────────────────────────────────────────
  IndexEntry indexes[MAX_INDEXED_TABLES];
  int indexCount;

  // ── Internal dispatch ─────────────────────────────────────────────────────
  void execCreate(const Token *toks, int count);
  void execDrop(const Token *toks, int count);
  void execList(const Token *toks, int count);
  void execInsert(const Token *toks, int count);
  void execSelect(const Token *toks, int count);
  void execUpdate(const Token *toks, int count);
  void execDelete(const Token *toks, int count);
  void execJoin(const Token *toks, int count);

  // ── Index helpers ─────────────────────────────────────────────────────────
  AVLTree *getOrBuildIndex(Table *table, const char *tableName);
  void buildIndex(Table *table, AVLTree *tree, int colIdx);

  // ── Scan / filter callbacks (static — used as function pointers) ──────────
  static bool whereFilter(const Row *row, void *ctx);
  static bool printMatch(const Row *row, void *ctx);

  // Disallow copy
  QueryExecutor(const QueryExecutor &);
  QueryExecutor &operator=(const QueryExecutor &);
};

#endif // QUERY_EXECUTOR_H
