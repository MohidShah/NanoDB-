/*
 * Table.h
 * -------
 * A Table manages one relation: it owns a Schema, interacts with the
 * BufferPool to store rows on pages, and provides INSERT / SCAN operations.
 *
 * ── Page layout per table ────────────────────────────────────────────────────
 *   Page 0..N belonging to this table are tracked in a page directory:
 *     int pageDirectory[MAX_TABLE_PAGES]  →  list of page IDs
 *
 *   Within each page (after the 8-byte header):
 *     [rowCount (4 bytes)] [row0 bytes] [row1 bytes] ...
 *     (rowCount is stored at page bytes 4-7 via Page::setRowCount)
 *
 * ── Persistence (Test Case G) ────────────────────────────────────────────────
 *   On shutdown, BufferPool::flushAll() writes all dirty pages to the .dbf
 *   file.  On startup, Table::load() reads the page directory from a small
 *   metadata file (tableName.meta) and re-registers pages with the pool.
 *   This proves the engine can survive a restart.
 *
 * ── Sequential scan ──────────────────────────────────────────────────────────
 *   scan() walks every page in the directory, deserializes each row,
 *   and applies a filter function pointer.  Complexity: O(N).
 *   This is intentionally slower than the AVL index (Test Case B).
 */
#ifndef TABLE_H
#define TABLE_H

#include "Schema.h"
#include "Row.h"
#include "../memory/BufferPool.h"
#include "../Logger.h"
// MAX_TABLE_PAGES, PAGE_SIZE, PAGE_HEADER_SZ come from Constants.h
// (transitively included via BufferPool.h -> Page.h -> Constants.h)

// Function pointer type for row filters (used by scan).
// Returns true if the row should be included in results.
typedef bool (*RowFilter)(const Row* row, void* ctx);

class Table {
public:
    Schema*     schema;                         // owned by Table
    BufferPool* pool;                           // NOT owned; shared across tables

    char  dataFile[256];                        // e.g. "customer.dbf"
    char  metaFile[256];                        // e.g. "customer.meta"

    int   pageDirectory[MAX_TABLE_PAGES];       // page IDs belonging to this table
    int   numPages;                             // entries used in pageDirectory
    int   totalRows;                            // total rows across all pages

    /**
     * Constructor — does NOT load data; call load() or startFresh() after.
     * @param schema   Table takes ownership of this pointer.
     * @param pool     Shared buffer pool (not owned).
     * @param dataFile Path to the binary page file.
     */
    Table(Schema* schema, BufferPool* pool, const char* dataFile);

    /** Destructor — frees schema; does NOT free the BufferPool. */
    ~Table();

    // ── Setup ─────────────────────────────────────────────────────────────────

    /** Call on first use — allocates page 0 for this table. */
    void startFresh();

    /**
     * Load page directory from the .meta file so we can find this table's
     * pages after an engine restart (Test Case G).
     */
    bool load();

    /**
     * Persist the page directory to the .meta file and flush all pages.
     * Called at engine shutdown.
     */
    void save();

    // ── DML ───────────────────────────────────────────────────────────────────

    /**
     * insertRow() — Append a row to the last page (or a new page if full).
     * @return The (pageId, slotId) where the row was stored.
     */
    bool insertRow(const Row* row, int* outPageId = nullptr, int* outSlotId = nullptr);

    /**
     * getRow() — Retrieve a specific row by (pageId, slotId).
     * Caller owns the returned Row* and must delete it.
     */
    Row* getRow(int pageId, int slotId);

    /**
     * updateRow() — Overwrite a row at (pageId, slotId) with new data.
     */
    bool updateRow(int pageId, int slotId, const Row* newRow);

    // ── Query ─────────────────────────────────────────────────────────────────

    /**
     * scan() — Sequential O(N) full-table scan.
     * For each row matching filter(row, ctx), calls onMatch(row, ctx).
     * @param filter    If nullptr, every row matches.
     * @param onMatch   Callback for matching rows. If nullptr, rows are printed.
     * @return Count of matching rows.
     */
    int scan(RowFilter filter, void* filterCtx,
             RowFilter onMatch = nullptr, void* matchCtx = nullptr);

    /** Print all rows (for debugging and demo output). */
    void printAll();

    int getRowCount() const { return totalRows; }

private:
    /** Allocate a new page for this table and add it to pageDirectory. */
    int allocatePage();

    // Disallow copy
    Table(const Table&);
    Table& operator=(const Table&);
};

#endif // TABLE_H
