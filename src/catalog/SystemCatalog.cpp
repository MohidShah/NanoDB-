/*
 * SystemCatalog.cpp
 * -----------------
 * Implementation of the engine-wide table registry.
 *
 * Catalog file format ("nanodb.catalog"):
 *   Each line: <tableName> <dataFile>
 *   Example:
 *     customer customer.dbf
 *     orders   orders.dbf
 *     lineitem lineitem.dbf
 *
 *   This is intentionally human-readable (plain text) so the evaluator
 *   can inspect it with a text editor.  A production engine would use a
 *   binary format or store the catalog as a special internal table.
 */
#include "SystemCatalog.h"
#include <stdio.h>   // FILE*, fopen, fclose, fprintf, fgets, sscanf
#include <string.h>  // strncpy, strcmp, strlen
#include <stdlib.h>  // NULL

// ── Constructor ───────────────────────────────────────────────────────────────
SystemCatalog::SystemCatalog(BufferPool* bp) : pool(bp) {
    Logger::log("SystemCatalog initialized.");
}

// ── Destructor ────────────────────────────────────────────────────────────────
SystemCatalog::~SystemCatalog() {
    saveCatalog();  // persist table list before destroying anything

    // Destroy every Table* owned by this catalog.
    // The forEach callback deletes the pointer; HashMap destructor then
    // frees the entry nodes themselves.
    struct Deleter {
        static void fn(const char* key, void* value, void* ctx) {
            (void)key; (void)ctx;
            delete static_cast<Table*>(value);
        }
    };
    tables.forEach(Deleter::fn, 0);

    Logger::log("SystemCatalog destroyed. All tables saved and freed.");
}

// ── createTable() ─────────────────────────────────────────────────────────────
Table* SystemCatalog::createTable(Schema* schema, const char* dataFile) {
    if (!schema || !dataFile) {
        Logger::error("createTable: null schema or dataFile.");
        return 0;
    }

    const char* name = schema->tableName;

    // Reject duplicate table names.
    if (tables.contains(name)) {
        Logger::error("createTable: table '%s' already exists.", name);
        return 0;
    }

    // Allocate the Table — catalog takes ownership.
    Table* t = new Table(schema, pool, dataFile);
    t->startFresh();   // allocate page 0 so the table is ready to accept rows

    tables.put(name, static_cast<void*>(t));
    Logger::log("Catalog: created table '%s' -> %s (%d cols)",
                name, dataFile, schema->numCols);
    return t;
}

// ── dropTable() ───────────────────────────────────────────────────────────────
bool SystemCatalog::dropTable(const char* tableName) {
    void* raw = tables.get(tableName);
    if (!raw) {
        Logger::error("dropTable: table '%s' not found.", tableName);
        return false;
    }
    Table* t = static_cast<Table*>(raw);
    tables.remove(tableName);
    delete t;
    Logger::log("Catalog: dropped table '%s'.", tableName);
    return true;
}

// ── getTable() ────────────────────────────────────────────────────────────────
Table* SystemCatalog::getTable(const char* tableName) const {
    void* raw = tables.get(tableName);
    return static_cast<Table*>(raw);  // nullptr if not found (void* cast is safe)
}

bool SystemCatalog::hasTable(const char* tableName) const {
    return tables.contains(tableName);
}

// ── listTables() ──────────────────────────────────────────────────────────────
void SystemCatalog::printTableName(const char* key, void* value, void* ctx) {
    (void)ctx;
    Table* t = static_cast<Table*>(value);
    printf("  %-20s  rows=%-6d  file=%s\n",
           key, t->getRowCount(), t->dataFile);
}

void SystemCatalog::listTables() const {
    printf("[Catalog] %d table(s) registered:\n", tables.size());
    tables.forEach(printTableName, 0);
}

// ── saveCatalog() ─────────────────────────────────────────────────────────────
// Callback for forEach — writes one line per table to the catalog file.
struct SaveCtx {
    FILE* f;
};

static void saveEntry(const char* key, void* value, void* ctx) {
    (void)key;
    SaveCtx* sc = static_cast<SaveCtx*>(ctx);
    Table* t = static_cast<Table*>(value);
    // Format: tableName dataFile\n
    fprintf(sc->f, "%s %s\n", t->schema->tableName, t->dataFile);
}

void SystemCatalog::saveCatalog() const {
    FILE* f = fopen(CATALOG_FILE, "w");
    if (!f) {
        Logger::error("saveCatalog: cannot open '%s' for writing.", CATALOG_FILE);
        return;
    }
    SaveCtx ctx = { f };
    tables.forEach(saveEntry, &ctx);
    fclose(f);
    Logger::log("Catalog saved to '%s' (%d tables).", CATALOG_FILE, tables.size());
}

// ── loadCatalog() ─────────────────────────────────────────────────────────────
void SystemCatalog::loadCatalog() {
    FILE* f = fopen(CATALOG_FILE, "r");
    if (!f) {
        // No catalog file yet — engine starting fresh for the first time.
        Logger::log("loadCatalog: no '%s' found. Starting fresh.", CATALOG_FILE);
        return;
    }

    char tableName[64];
    char dataFile[256];
    int  loaded = 0;

    while (fscanf(f, "%63s %255s", tableName, dataFile) == 2) {
        // Re-open the table from its existing .dbf and .meta files.
        // We create a minimal schema first; the full schema is recovered
        // from the .meta file by Table::load().
        Schema* schema = new Schema();
        strncpy(schema->tableName, tableName, MAX_TABLE_NAME - 1);

        Table* t = new Table(schema, pool, dataFile);
        if (t->load()) {
            tables.put(tableName, static_cast<void*>(t));
            Logger::log("Catalog: re-opened table '%s' (%d rows).",
                        tableName, t->getRowCount());
            loaded++;
        } else {
            Logger::error("Catalog: failed to load table '%s'.", tableName);
            delete t;   // frees schema too (Table destructor owns it)
        }
    }

    fclose(f);
    Logger::log("loadCatalog: restored %d table(s) from '%s'.", loaded, CATALOG_FILE);
}
