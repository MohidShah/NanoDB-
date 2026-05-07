/*
 * SystemCatalog.h
 * ---------------
 * The SystemCatalog is NanoDB's engine-wide table registry.
 *
 * In a real RDBMS (PostgreSQL: pg_class, MySQL: information_schema),
 * the catalog stores metadata about every table, column, index, and
 * constraint.  Our simplified catalog stores:
 *
 *   tableName (string)  →  Table* (heap-allocated, owned by catalog)
 *
 * ── Why a separate Catalog layer? ───────────────────────────────────────────
 *   The query parser and executor need to resolve table names to their
 *   Schema objects before they can plan or execute a query.  Without a
 *   catalog, every component would need its own table-name lookup logic.
 *   Centralising this in SystemCatalog follows the Single Responsibility
 *   Principle (one place to create, find, and destroy Table objects).
 *
 * ── Persistence (Test Case G) ────────────────────────────────────────────────
 *   On shutdown: saveCatalog() writes all table names + data-file paths to
 *   a plain-text file ("nanodb.catalog").  On startup: loadCatalog() reads
 *   this file and re-opens every Table.  This means the engine can survive
 *   a process restart and still answer queries.
 *
 * ── Ownership ────────────────────────────────────────────────────────────────
 *   SystemCatalog owns every Table* it creates.
 *   The destructor calls save() then deletes all Table objects.
 *   BufferPool is NOT owned by the catalog; it is shared.
 *
 * Viva Q: "How does the parser resolve 'SELECT * FROM orders'?"
 *   The executor calls SystemCatalog::getTable("orders"), which does an
 *   O(1) HashMap lookup and returns the Table* (or nullptr if not found,
 *   triggering an error message).
 */
#ifndef SYSTEM_CATALOG_H
#define SYSTEM_CATALOG_H

#include "HashMap.h"
#include "../schema/Table.h"
#include "../Logger.h"
#include <stdio.h>   // FILE*, fopen, fclose, fprintf, fgets
#include <string.h>  // strncpy, strcmp, strlen

// Name of the catalog file persisted to disk.
static const char CATALOG_FILE[] = "nanodb.catalog";

class SystemCatalog {
public:
    /**
     * Constructor.
     * @param pool  Shared BufferPool — all tables created by the catalog
     *              use this pool for page I/O.  NOT owned by the catalog.
     */
    explicit SystemCatalog(BufferPool* pool);

    /**
     * Destructor — saves the catalog to disk, then destroys all Table objects.
     */
    ~SystemCatalog();

    // ── DDL ───────────────────────────────────────────────────────────────────

    /**
     * createTable() — Define a new table and register it in the catalog.
     * Creates a fresh Table (startFresh()) and persists its schema in memory.
     * Returns the newly created Table* (owned by catalog), or nullptr on error.
     *
     * @param schema   Caller passes a heap-allocated Schema*; the catalog
     *                 takes ownership.
     * @param dataFile Path to the binary page file for this table.
     */
    Table* createTable(Schema* schema, const char* dataFile);

    /**
     * dropTable() — Remove a table from the catalog and delete it.
     * Also removes the in-memory Table object.
     * Returns true if the table existed and was removed.
     */
    bool dropTable(const char* tableName);

    // ── Query ─────────────────────────────────────────────────────────────────

    /**
     * getTable() — O(1) lookup by table name.
     * Returns the Table* if found, nullptr otherwise.
     */
    Table* getTable(const char* tableName) const;

    /** Returns true if a table with this name is registered. */
    bool   hasTable(const char* tableName) const;

    /** Number of tables currently registered in the catalog. */
    int    tableCount() const { return tables.size(); }

    /** Print the name of every registered table to stdout. */
    void   listTables() const;

    // ── Persistence ───────────────────────────────────────────────────────────

    /**
     * saveCatalog() — Write all table names and data-file paths to
     * "nanodb.catalog" (plain text, one table per line).
     * Called automatically by the destructor.
     */
    void saveCatalog() const;

    /**
     * loadCatalog() — Read "nanodb.catalog" and re-open all tables.
     * Each table's page data is re-loaded from its .dbf file via load().
     * Call this once at engine startup after creating the SystemCatalog.
     */
    void loadCatalog();

private:
    HashMap     tables;    // tableName → Table* (owned by this catalog)
    BufferPool* pool;      // shared buffer pool (NOT owned)

    // forEach callback for listTables
    static void printTableName(const char* key, void* value, void* ctx);

    // Disallow copy
    SystemCatalog(const SystemCatalog&);
    SystemCatalog& operator=(const SystemCatalog&);
};

#endif // SYSTEM_CATALOG_H
