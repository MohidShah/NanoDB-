/*
 * main.cpp
 * --------
 * NanoDB entry point.
 *
 * Start-up sequence
 * -----------------
 *   1. Open logger  (nanodb_execution.log)
 *   2. Create BufferPool (MAX_PAGES = 50 for Test Case D stress-test)
 *   3. Create SystemCatalog  → loadCatalog() (Test Case G: persistence)
 *   4. Create QueryExecutor
 *   5. Define schemas (customer / orders / lineitem) if not already on disk
 *   6. Load TPC-H rows from pipe-delimited .tbl files into the tables
 *   7. Submit all queries from queries.txt to the priority queue
 *   8. runAll()  — drains the queue; admin queries execute first (Test Case E)
 *   9. Report total LRU evictions (Test Case D)
 *  10. Flush & close
 */

#include "engine/QueryExecutor.h"
#include "memory/BufferPool.h"
#include "catalog/SystemCatalog.h"
#include "Logger.h"
#include "Constants.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// ── Compile-time config ────────────────────────────────────────────────────
static const int  POOL_MAX_PAGES  = 50;        // small pool → forces LRU evictions
static const char LOG_FILE[]      = "nanodb_execution.log";
static const char QUERIES_FILE[]  = "queries.txt";
static const char CUSTOMER_TBL[]  = "data/customer.tbl";
static const char ORDERS_TBL[]    = "data/orders.tbl";
static const char LINEITEM_TBL[]  = "data/lineitem.tbl";

// ── Forward declarations ───────────────────────────────────────────────────
static void buildSchemas(SystemCatalog* cat, BufferPool* pool);
static void loadTbl(QueryExecutor* exec, const char* tblFile, const char* tableName);
static void runQueriesFile(QueryExecutor* exec, const char* path);

// ─────────────────────────────────────────────────────────────────────────────
int main() {
    // 1. Logger
    Logger::init(LOG_FILE);
    Logger::separator("NanoDB STARTUP");

    // 2. Buffer pool (shared across all tables)
    BufferPool pool(POOL_MAX_PAGES, "data/nanodb.dbf");

    // 3. System catalog
    SystemCatalog catalog(&pool);
    catalog.loadCatalog();   // no-op if nanodb.catalog doesn't exist yet

    // 4. Query executor
    QueryExecutor exec(&pool, &catalog);

    // 5. Define schemas (idempotent — skipped if tables already in catalog)
    buildSchemas(&catalog, &pool);

    // 6. Load TPC-H data
    Logger::separator("Loading TPC-H data");
    loadTbl(&exec, CUSTOMER_TBL, "customer");
    loadTbl(&exec, ORDERS_TBL,   "orders");
    loadTbl(&exec, LINEITEM_TBL, "lineitem");

    // 7 & 8. Submit + execute all queries
    Logger::separator("Executing workload queries");
    runQueriesFile(&exec, QUERIES_FILE);
    exec.runAll();

    // 9. Test Case D: LRU eviction report
    Logger::separator("Buffer Pool Statistics");
    Logger::log("Total LRU page evictions: %d", pool.getEvictionCount());

    // 10. Flush & close
    pool.flushAll();
    Logger::separator("NanoDB SHUTDOWN");
    Logger::close();
    return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// buildSchemas — register customer / orders / lineitem if not already present
// ─────────────────────────────────────────────────────────────────────────────
static void buildSchemas(SystemCatalog* cat, BufferPool* /*pool*/) {
    // customer: c_custkey|c_name|c_address|c_nationkey|c_phone|c_acctbal|c_mktsegment|c_comment
    if (!cat->hasTable("customer")) {
        Schema* s = new Schema();
        strncpy(s->tableName, "customer", MAX_TABLE_NAME-1);
        s->addColumn("c_custkey",   Field::INT);
        s->addColumn("c_name",      Field::STRING);
        s->addColumn("c_address",   Field::STRING);
        s->addColumn("c_nationkey", Field::INT);
        s->addColumn("c_phone",     Field::STRING);
        s->addColumn("c_acctbal",   Field::FLOAT);
        s->addColumn("c_mktsegment",Field::STRING);
        s->addColumn("c_comment",   Field::STRING);
        cat->createTable(s, "data/customer.dbf");
        Logger::log("Schema created: customer (8 columns).");
    }

    // orders: o_orderkey|o_custkey|o_orderstatus|o_totalprice|o_orderdate|o_orderpriority|o_clerk|o_shippriority|o_comment
    if (!cat->hasTable("orders")) {
        Schema* s = new Schema();
        strncpy(s->tableName, "orders", MAX_TABLE_NAME-1);
        s->addColumn("o_orderkey",    Field::INT);
        s->addColumn("o_custkey",     Field::INT);
        s->addColumn("o_orderstatus", Field::STRING);
        s->addColumn("o_totalprice",  Field::FLOAT);
        s->addColumn("o_orderdate",   Field::STRING);
        s->addColumn("o_orderpriority",Field::STRING);
        s->addColumn("o_clerk",       Field::STRING);
        s->addColumn("o_shippriority",Field::INT);
        s->addColumn("o_comment",     Field::STRING);
        cat->createTable(s, "data/orders.dbf");
        Logger::log("Schema created: orders (9 columns).");
    }

    // lineitem: l_orderkey|l_partkey|l_suppkey|l_linenumber|l_quantity|l_extendedprice|...
    if (!cat->hasTable("lineitem")) {
        Schema* s = new Schema();
        strncpy(s->tableName, "lineitem", MAX_TABLE_NAME-1);
        s->addColumn("l_orderkey",      Field::INT);
        s->addColumn("l_partkey",       Field::INT);
        s->addColumn("l_suppkey",       Field::INT);
        s->addColumn("l_linenumber",    Field::INT);
        s->addColumn("l_quantity",      Field::FLOAT);
        s->addColumn("l_extendedprice", Field::FLOAT);
        s->addColumn("l_discount",      Field::FLOAT);
        s->addColumn("l_tax",           Field::FLOAT);
        s->addColumn("l_returnflag",    Field::STRING);
        s->addColumn("l_linestatus",    Field::STRING);
        s->addColumn("l_shipdate",      Field::STRING);
        s->addColumn("l_commitdate",    Field::STRING);
        s->addColumn("l_receiptdate",   Field::STRING);
        s->addColumn("l_shipinstruct",  Field::STRING);
        s->addColumn("l_shipmode",      Field::STRING);
        s->addColumn("l_comment",       Field::STRING);
        cat->createTable(s, "data/lineitem.dbf");
        Logger::log("Schema created: lineitem (16 columns).");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// loadTbl — parse a pipe-delimited .tbl file and insert rows into a table.
// Each field is separated by '|'; trailing '|' is allowed.
// ─────────────────────────────────────────────────────────────────────────────
static void loadTbl(QueryExecutor* exec, const char* tblFile, const char* tableName) {
    FILE* f = fopen(tblFile, "r");
    if (!f) {
        Logger::log("loadTbl: '%s' not found, skipping.", tblFile);
        return;
    }

    char line[4096];
    int  loaded = 0;
    char sql[4096];

    while (fgets(line, sizeof(line), f)) {
        // strip trailing newline / carriage-return
        int len = (int)strlen(line);
        while (len > 0 && (line[len-1]=='\n'||line[len-1]=='\r'||line[len-1]=='|'))
            line[--len] = '\0';
        if (len == 0) continue;

        // Build: INSERT INTO tableName VALUES (v1, v2, ...)
        int off = snprintf(sql, sizeof(sql), "INSERT INTO %s VALUES (", tableName);
        char* tok = strtok(line, "|");
        bool first = true;
        while (tok) {
            if (!first) off += snprintf(sql+off, sizeof(sql)-off, ", ");
            first = false;
            // Wrap strings in quotes if they contain non-numeric chars
            bool isNum = true;
            bool hasDot = false;
            for (int i = 0; tok[i]; i++) {
                if (tok[i] == '.' && !hasDot) { hasDot = true; continue; }
                if (tok[i] == '-' && i==0)    continue;
                if (tok[i] < '0' || tok[i] > '9') { isNum = false; break; }
            }
            if (isNum && strlen(tok) > 0)
                off += snprintf(sql+off, sizeof(sql)-off, "%s", tok);
            else
                off += snprintf(sql+off, sizeof(sql)-off, "'%s'", tok);
            tok = strtok(0, "|");
        }
        snprintf(sql+off, sizeof(sql)-off, ")");
        exec->execute(sql);
        loaded++;
    }
    fclose(f);
    Logger::log("loadTbl: %d rows loaded into '%s'.", loaded, tableName);
}

// ─────────────────────────────────────────────────────────────────────────────
// runQueriesFile — read queries.txt and submit each line to the executor queue
// ─────────────────────────────────────────────────────────────────────────────
static void runQueriesFile(QueryExecutor* exec, const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) {
        Logger::error("Cannot open '%s'.", path);
        return;
    }
    char line[1024];
    int  count = 0;
    while (fgets(line, sizeof(line), f)) {
        // strip newline; skip comments and blank lines
        int len = (int)strlen(line);
        while (len > 0 && (line[len-1]=='\n'||line[len-1]=='\r')) line[--len] = '\0';
        if (len == 0 || line[0] == '-' || line[0] == '#') continue;
        exec->submit(line);
        count++;
    }
    fclose(f);
    Logger::log("Queued %d statements from %s.", count, path);
}
