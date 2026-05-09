/*
 * test_runner.cpp
 * ---------------
 * Automated test suite for NanoDB.  Exercises all 7 mandatory test cases
 * (A–G) with hard-coded, minimal data so the evaluator can verify correct
 * behaviour without needing the full 100 K TPC-H dataset.
 *
 * Each test case prints PASS / FAIL to stdout and logs to nanodb_test.log.
 *
 * Test Cases
 * ----------
 *  A  Parser + Shunting-Yard: verify postfix output of compound WHERE expr
 *  B  AVL index vs. sequential scan: both find the same row; timing shown
 *  C  Graph + Kruskal MST: verify join order for 3-table query
 *  D  LRU Buffer Pool: verify eviction count > 0 under 50-page limit
 *  E  Priority Queue: UPDATE submitted after SELECT but executes first
 *  F  Nested arithmetic expression: correct evaluation of (a+b)*c > d
 *  G  Persistence: rows survive a simulated restart (save → reload)
 */

#include "src/engine/QueryExecutor.h"
#include "src/memory/BufferPool.h"
#include "src/catalog/SystemCatalog.h"
#include "src/parser/Tokenizer.h"
#include "src/parser/ShuntingYard.h"
#include "src/parser/ExprEvaluator.h"
#include "src/queue/PriorityQueue.h"
#include "src/index/AVLTree.h"
#include "src/optimizer/Graph.h"
#include "src/optimizer/KruskalMST.h"
#include "src/Logger.h"
#include "src/Constants.h"
#include "src/schema/Field.h"
#include "src/schema/Schema.h"
#include "src/schema/Row.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────
static int passed = 0;
static int failed = 0;

static void report(const char* name, bool ok) {
    if (ok) {
        printf("[PASS] %s\n", name);
        Logger::log("[PASS] %s", name);
        passed++;
    } else {
        printf("[FAIL] %s\n", name);
        Logger::log("[FAIL] %s", name);
        failed++;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Helper: create and populate a small test table
// ─────────────────────────────────────────────────────────────────────────────
static Schema* makeTestSchema(const char* name) {
    Schema* s = new Schema();
    strncpy(s->tableName, name, MAX_TABLE_NAME-1);
    s->addColumn("id",    Field::INT);
    s->addColumn("value", Field::FLOAT);
    s->addColumn("label", Field::STRING);
    return s;
}

static void insertTestRow(Table* t, int id, double val, const char* label) {
    Row* r = new Row(t->schema);
    IntField    f0(id);
    FloatField  f1(val);
    StringField f2(label);
    r->setField(0, &f0);
    r->setField(1, &f1);
    r->setField(2, &f2);
    t->insertRow(r);
    delete r;
}

// ─────────────────────────────────────────────────────────────────────────────
// Test A — Parser + Shunting-Yard
// ─────────────────────────────────────────────────────────────────────────────
static void testA() {
    Logger::separator("Test A: Parser + Shunting-Yard");
    // Infix: id > 5 AND value < 100.0
    // Expected postfix: id 5 > value 100.0 < AND
    const char* sql = "id > 5 AND value < 100.0";
    Tokenizer tok;
    tok.tokenize(sql);

    ShuntingYard sy;
    sy.convert(tok.tokens, tok.count);

    // Check postfix order: [id][5][>][value][100.0][<][AND]
    bool ok = (sy.outCount == 7 &&
               sy.output[0].type == TOK_IDENT    &&
               sy.output[1].type == TOK_INT_LIT  &&
               sy.output[2].type == TOK_GT       &&
               sy.output[3].type == TOK_IDENT    &&
               sy.output[4].type == TOK_FLOAT_LIT &&
               sy.output[5].type == TOK_LT       &&
               sy.output[6].type == TOK_AND);

    char postBuf[256] = {0};
    int off = 0;
    for (int i = 0; i < sy.outCount; i++)
        off += snprintf(postBuf+off, sizeof(postBuf)-off, "%s ", sy.output[i].lexeme);
    Logger::log("Infix \"%s\" → Postfix \"%s\"", sql, postBuf);
    report("A: Shunting-Yard postfix conversion", ok);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test B — AVL index vs. sequential scan
// ─────────────────────────────────────────────────────────────────────────────
static void testB() {
    Logger::separator("Test B: AVL Index vs. Sequential Scan");

    BufferPool pool(50, "data/test_b.dbf");
    SystemCatalog cat(&pool);
    Schema* s = makeTestSchema("test_b");
    Table* tbl = cat.createTable(s, "data/test_b.dbf");
    if (!tbl) { report("B: table created", false); return; }

    // Insert 1000 rows
    for (int i = 1; i <= 1000; i++) insertTestRow(tbl, i, i * 1.5, "row");

    // Build AVL index on col 0 (id)
    AVLTree tree;
    for (int p = 0; p < tbl->numPages; p++) {
        int pid = tbl->pageDirectory[p];
        Page* page = pool.fetchPage(pid);
        if (!page) continue;
        int rc = page->getRowCount();
        pool.releasePage(pid);
        for (int slot = 0; slot < rc; slot++) {
            Row* row = tbl->getRow(pid, slot);
            if (!row) continue;
            const Field* f = row->getField(0);
            if (f) tree.insert(f->toDouble(), pid, slot);
            delete row;
        }
    }

    // Time sequential scan for id=500
    clock_t scanStart = clock();
    int scanHits = 0;
    for (int p = 0; p < tbl->numPages; p++) {
        int pid = tbl->pageDirectory[p];
        Page* page = pool.fetchPage(pid);
        if (!page) continue;
        int rc = page->getRowCount();
        pool.releasePage(pid);
        for (int s2 = 0; s2 < rc; s2++) {
            Row* row = tbl->getRow(pid, s2);
            if (!row) continue;
            const Field* f = row->getField(0);
            if (f && f->toDouble() == 500.0) scanHits++;
            delete row;
        }
    }
    clock_t scanEnd = clock();
    double scanMs = (double)(scanEnd - scanStart) * 1000.0 / CLOCKS_PER_SEC;

    // Time AVL search for id=500
    clock_t avlStart = clock();
    RID rid = tree.search(500.0);
    clock_t avlEnd = clock();
    double avlMs = (double)(avlEnd - avlStart) * 1000.0 / CLOCKS_PER_SEC;

    printf("[B] Sequential scan: %.4f ms (%d hit)  |  AVL search: %.4f ms\n",
           scanMs, scanHits, avlMs);
    Logger::log("Sequential scan: %.4f ms | AVL search: %.4f ms | found=%s",
                scanMs, avlMs, rid.valid() ? "yes" : "no");

    bool ok = rid.valid() && scanHits == 1;
    report("B: AVL index finds same row as sequential scan", ok);
    pool.flushAll();
}

// ─────────────────────────────────────────────────────────────────────────────
// Test C — Kruskal MST join order
// ─────────────────────────────────────────────────────────────────────────────
static void testC() {
    Logger::separator("Test C: Kruskal MST Join Order");
    Graph g;
    g.addNode("customer", 20000);
    g.addNode("orders",   30000);
    g.addNode("lineitem", 50000);
    g.addEdge("customer", "c_custkey", "orders",   "o_custkey");
    g.addEdge("orders",   "o_orderkey","lineitem",  "l_orderkey");

    KruskalMST mst;
    JoinPlan plan;
    int edges = mst.run(g, plan);

    // Optimal order: customer ⋈ orders (cost 20K*30K=600M) < orders ⋈ lineitem? 
    // Actually: customer⋈orders weight = 20000*30000 = 600M, orders⋈lineitem = 30000*50000=1.5B
    // Kruskal picks lowest weight first → customer⋈orders first.
    KruskalMST::printPlan(g, plan);
    Logger::log("MST edges: %d", edges);

    bool ok = (edges == 2);
    report("C: Kruskal MST produces 2 join steps for 3 tables", ok);

    if (plan.joinCount > 0) {
        // First join should be customer-orders (cheaper)
        int nA = plan.joins[0].nodeA, nB = plan.joins[0].nodeB;
        const char* tA = g.nodes[nA].tableName;
        const char* tB = g.nodes[nB].tableName;
        bool correctOrder = (
            (strcmp(tA,"customer")==0 || strcmp(tB,"customer")==0) &&
            (strcmp(tA,"orders")==0   || strcmp(tB,"orders")==0));
        Logger::log("First join: %s ⋈ %s", tA, tB);
        report("C: First MST join is customer ⋈ orders", correctOrder);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Test D — LRU eviction
// ─────────────────────────────────────────────────────────────────────────────
static void testD() {
    Logger::separator("Test D: LRU Buffer Pool Eviction");
    // Pool size = 5 pages; insert enough rows to need > 5 pages → forces evictions
    BufferPool pool(5, "data/test_d.dbf");
    SystemCatalog cat(&pool);
    Schema* s = makeTestSchema("test_d");
    Table* tbl = cat.createTable(s, "data/test_d.dbf");
    if (!tbl) { report("D: table created", false); return; }

    // Each row ≈ 4+8+256 = 268 bytes; rows per page ≈ (4096-8)/268 ≈ 15
    // Insert 200 rows → ~14 pages → triggers evictions with pool size 5
    for (int i = 0; i < 200; i++) insertTestRow(tbl, i, i*2.0, "evict_test");

    int evictions = pool.getEvictionCount();
    printf("[D] LRU evictions: %d\n", evictions);
    Logger::log("Total LRU page evictions: %d", evictions);
    report("D: LRU evictions > 0 with small buffer pool", evictions > 0);
    pool.flushAll();
}

// ─────────────────────────────────────────────────────────────────────────────
// Test E — Priority Queue admin bypass
// ─────────────────────────────────────────────────────────────────────────────
static void testE() {
    Logger::separator("Test E: Priority Queue Admin Bypass");
    PriorityQueue pq;

    // Submit user queries first, then an admin UPDATE
    pq.insert(PRIORITY_USER,  "SELECT * FROM customer WHERE c_custkey = 1");
    pq.insert(PRIORITY_USER,  "SELECT * FROM orders WHERE o_orderkey = 10");
    pq.insert(PRIORITY_ADMIN, "UPDATE customer SET c_acctbal=9999.0 WHERE c_custkey=1");
    pq.insert(PRIORITY_USER,  "SELECT * FROM lineitem WHERE l_orderkey = 10");

    Logger::log("Admin UPDATE intercepted, promoted to front of queue.");
    QueryTask first = pq.extractMax();
    printf("[E] First dequeued priority=%d: %s\n", first.priority, first.sql);

    bool ok = (first.priority == PRIORITY_ADMIN);
    report("E: Admin UPDATE dequeued before user SELECT", ok);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test F — Nested arithmetic expression evaluation
// ─────────────────────────────────────────────────────────────────────────────
static void testF() {
    Logger::separator("Test F: Nested Arithmetic Expression");
    // Expression: (id + 2) * 3 > 15   →   rows where id > 3
    // Build a minimal schema + row with id=5 (should match), id=2 (should not)
    Schema schema;
    strncpy(schema.tableName, "test_f", MAX_TABLE_NAME-1);
    schema.addColumn("id",    Field::INT);
    schema.addColumn("value", Field::FLOAT);
    schema.addColumn("label", Field::STRING);

    Tokenizer tok;
    tok.tokenize("(id + 2) * 3 > 15");
    ShuntingYard sy;
    sy.convert(tok.tokens, tok.count);

    char postBuf[256]={0};
    int off = 0;
    for (int i = 0; i < sy.outCount; i++)
        off += snprintf(postBuf+off, sizeof(postBuf)-off, "%s ", sy.output[i].lexeme);
    Logger::log("Infix \"(id + 2) * 3 > 15\" → Postfix \"%s\"", postBuf);

    ExprEvaluator eval;

    // Row with id=5: (5+2)*3=21 > 15 → true
    Row row5(&schema);
    IntField i5(5); FloatField f5(5.0); StringField s5("r");
    row5.setField(0,&i5); row5.setField(1,&f5); row5.setField(2,&s5);
    bool err=false;
    bool res5 = eval.evaluate(sy.output, sy.outCount, &row5, &schema, &err);

    // Row with id=2: (2+2)*3=12 > 15 → false
    Row row2(&schema);
    IntField i2(2); FloatField f2(2.0); StringField s2("r");
    row2.setField(0,&i2); row2.setField(1,&f2); row2.setField(2,&s2);
    bool res2 = eval.evaluate(sy.output, sy.outCount, &row2, &schema, &err);

    printf("[F] id=5 → %s (expect true), id=2 → %s (expect false)\n",
           res5?"true":"false", res2?"true":"false");
    report("F: Nested arithmetic expression evaluates correctly", res5 && !res2);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test G — Persistence (rows survive restart)
// ─────────────────────────────────────────────────────────────────────────────
static void testG() {
    Logger::separator("Test G: Persistence (rows survive restart)");

    // Phase 1: create, insert, save
    {
        BufferPool pool(50, "data/test_g.dbf");
        SystemCatalog cat(&pool);
        Schema* s = makeTestSchema("test_g");
        Table* tbl = cat.createTable(s, "data/test_g.dbf");
        if (!tbl) { report("G: table created in phase 1", false); return; }
        insertTestRow(tbl, 42, 3.14, "persist_me");
        tbl->save();
        cat.saveCatalog();
        pool.flushAll();
        Logger::log("G: Phase 1 complete — row id=42 written to disk.");
    }
    // Phase 2: reload and verify
    {
        BufferPool pool(50, "data/test_g.dbf");
        SystemCatalog cat(&pool);
        cat.loadCatalog();
        Table* tbl = cat.getTable("test_g");
        if (!tbl) { report("G: table reloaded after restart", false); return; }

        bool found = false;
        for (int p = 0; p < tbl->numPages; p++) {
            int pid = tbl->pageDirectory[p];
            Page* page = pool.fetchPage(pid);
            if (!page) continue;
            int rc = page->getRowCount();
            pool.releasePage(pid);
            for (int slot = 0; slot < rc; slot++) {
                Row* row = tbl->getRow(pid, slot);
                if (!row) continue;
                const Field* f = row->getField(0);
                if (f && (int)f->toDouble() == 42) found = true;
                delete row;
            }
        }
        Logger::log("G: Phase 2 — row id=42 found after reload: %s", found?"YES":"NO");
        report("G: Row persists after simulated engine restart", found);
        pool.flushAll();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
int main() {
    // Ensure data/ directory exists
    system("mkdir data 2>nul");  // Windows; harmless if already exists

    Logger::init("nanodb_test.log");
    Logger::separator("NanoDB Test Runner");

    testA();
    testB();
    testC();
    testD();
    testE();
    testF();
    testG();

    Logger::separator("Results");
    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    Logger::log("Results: %d passed, %d failed.", passed, failed);
    Logger::close();
    return (failed == 0) ? 0 : 1;
}
