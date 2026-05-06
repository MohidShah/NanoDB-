/*
 * Table.cpp
 * ---------
 * Table implementation — row storage and retrieval via the Buffer Pool.
 *
 * Page slot addressing:
 *   A "slot" is the 0-based index of a row within one page.
 *   Row byte offset on page = PAGE_HEADER_SZ + (slotId * schema->rowWidth())
 *
 * Persistence (.meta file format):
 *   Line 0: totalRows  (int as ASCII)
 *   Line 1: numPages   (int as ASCII)
 *   Lines 2..N: one pageId per line
 *   This is intentionally plain text so it's human-readable during debugging.
 */
#include "Table.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>  // atoi

// ── Constructor ───────────────────────────────────────────────────────────────
Table::Table(Schema* sch, BufferPool* bp, const char* df)
    : schema(sch), pool(bp), numPages(0), totalRows(0)
{
    std::strncpy(dataFile, df, 255);
    dataFile[255] = '\0';

    // Derive meta file name: replace .dbf with .meta
    std::strncpy(metaFile, df, 250);
    metaFile[250] = '\0';
    // Find last '.' and replace suffix
    char* dot = nullptr;
    for (char* p = metaFile; *p; p++) if (*p == '.') dot = p;
    if (dot) std::strcpy(dot, ".meta");
    else     std::strcat(metaFile, ".meta");

    std::memset(pageDirectory, -1, sizeof(pageDirectory));

    Logger::log("Table [%s] initialized. dataFile=%s", schema->tableName, dataFile);
}

// ── Destructor ────────────────────────────────────────────────────────────────
Table::~Table() {
    delete schema;
    // pool is NOT deleted — it's shared and managed externally
}

// ── startFresh ────────────────────────────────────────────────────────────────
void Table::startFresh() {
    numPages  = 0;
    totalRows = 0;
    allocatePage();   // allocate page 0
    Logger::log("Table [%s]: started fresh, page 0 allocated.", schema->tableName);
}

// ── allocatePage ─────────────────────────────────────────────────────────────
int Table::allocatePage() {
    if (numPages >= MAX_TABLE_PAGES) {
        Logger::error("Table [%s]: page directory full!", schema->tableName);
        return -1;
    }
    Page* p = pool->newPage();
    int pageId = p->pageId;
    p->setRowCount(0);
    pool->releasePage(pageId);
    pageDirectory[numPages++] = pageId;
    Logger::log("Table [%s]: allocated page %d (slot %d in directory).",
                schema->tableName, pageId, numPages - 1);
    return pageId;
}

// ── load (persistence — Test Case G) ─────────────────────────────────────────
bool Table::load() {
    FILE* f = fopen(metaFile, "r");
    if (!f) {
        Logger::log("Table [%s]: no meta file found (%s). Starting fresh.",
                    schema->tableName, metaFile);
        startFresh();
        return false;
    }
    // Read totalRows and numPages
    char line[64];
    fgets(line, sizeof(line), f); totalRows = atoi(line);
    fgets(line, sizeof(line), f); numPages  = atoi(line);
    // Read each page ID
    for (int i = 0; i < numPages; i++) {
        fgets(line, sizeof(line), f);
        pageDirectory[i] = atoi(line);
    }
    fclose(f);
    Logger::log("Table [%s]: loaded from meta. totalRows=%d, numPages=%d.",
                schema->tableName, totalRows, numPages);
    return true;
}

// ── save (persistence — Test Case G) ─────────────────────────────────────────
void Table::save() {
    pool->flushAll();   // write all dirty pages to disk first

    FILE* f = fopen(metaFile, "w");
    if (!f) {
        Logger::error("Table [%s]: cannot write meta file %s", schema->tableName, metaFile);
        return;
    }
    fprintf(f, "%d\n", totalRows);
    fprintf(f, "%d\n", numPages);
    for (int i = 0; i < numPages; i++) {
        fprintf(f, "%d\n", pageDirectory[i]);
    }
    fclose(f);
    Logger::log("Table [%s]: saved meta file. totalRows=%d, numPages=%d.",
                schema->tableName, totalRows, numPages);
}

// ── insertRow ─────────────────────────────────────────────────────────────────
bool Table::insertRow(const Row* row, int* outPageId, int* outSlotId) {
    int rw = schema->rowWidth();
    int rpp = schema->rowsPerPage();

    // Find the last page and check if it has space.
    int lastDirIdx  = numPages - 1;
    int lastPageId  = pageDirectory[lastDirIdx];

    Page* p = pool->fetchPage(lastPageId);
    int rowCount = p->getRowCount();

    if (rowCount >= rpp) {
        // Page is full — allocate a new one.
        pool->releasePage(lastPageId);
        lastPageId = allocatePage();
        if (lastPageId < 0) return false;
        p = pool->fetchPage(lastPageId);
        rowCount = 0;
    }

    // Serialize the row into a temporary buffer.
    char* rowBuf = new char[rw];
    row->serialize(rowBuf, schema);

    // Write the row bytes at the correct offset within the page.
    // Offset = header (8 bytes) + slotId * rowWidth
    int offset = PAGE_HEADER_SZ + rowCount * rw;
    p->write(rowBuf, offset, rw);
    delete[] rowBuf;

    // Update the row count stored in the page header.
    p->setRowCount(rowCount + 1);

    if (outPageId) *outPageId = lastPageId;
    if (outSlotId) *outSlotId = rowCount;

    pool->releasePage(lastPageId);
    totalRows++;
    return true;
}

// ── getRow ────────────────────────────────────────────────────────────────────
Row* Table::getRow(int pageId, int slotId) {
    Page* p = pool->fetchPage(pageId);
    if (!p) return nullptr;

    int rw     = schema->rowWidth();
    int offset = PAGE_HEADER_SZ + slotId * rw;

    char* rowBuf = new char[rw];
    p->read(rowBuf, offset, rw);
    pool->releasePage(pageId);

    Row* row = new Row(schema);
    row->deserialize(rowBuf, schema);
    delete[] rowBuf;
    return row;
}

// ── updateRow ─────────────────────────────────────────────────────────────────
bool Table::updateRow(int pageId, int slotId, const Row* newRow) {
    Page* p = pool->fetchPage(pageId);
    if (!p) return false;

    int rw     = schema->rowWidth();
    int offset = PAGE_HEADER_SZ + slotId * rw;

    char* rowBuf = new char[rw];
    newRow->serialize(rowBuf, schema);
    p->write(rowBuf, offset, rw);
    delete[] rowBuf;

    pool->releasePage(pageId);
    Logger::log("Table [%s]: updated row at page=%d slot=%d.",
                schema->tableName, pageId, slotId);
    return true;
}

// ── scan ──────────────────────────────────────────────────────────────────────
int Table::scan(RowFilter filter, void* filterCtx,
                RowFilter onMatch, void* matchCtx)
{
    int matchCount = 0;
    int rw         = schema->rowWidth();
    char* rowBuf   = new char[rw];

    for (int di = 0; di < numPages; di++) {
        int pageId = pageDirectory[di];
        Page* p    = pool->fetchPage(pageId);
        int rowCount = p->getRowCount();

        for (int slot = 0; slot < rowCount; slot++) {
            int offset = PAGE_HEADER_SZ + slot * rw;
            p->read(rowBuf, offset, rw);

            Row* row = new Row(schema);
            row->deserialize(rowBuf, schema);

            // Apply filter (nullptr = match all)
            bool matches = (filter == nullptr) || filter(row, filterCtx);

            if (matches) {
                matchCount++;
                if (onMatch) {
                    onMatch(row, matchCtx);
                } else {
                    row->print();
                }
            }

            delete row;
        }

        pool->releasePage(pageId);
    }

    delete[] rowBuf;
    return matchCount;
}

// ── printAll ──────────────────────────────────────────────────────────────────
void Table::printAll() {
    printf("\n=== Table: %s (%d rows) ===\n", schema->tableName, totalRows);
    // Print header
    printf("| ");
    for (int i = 0; i < schema->numCols; i++) {
        printf("%-20s | ", schema->columns[i].name);
    }
    printf("\n");
    scan(nullptr, nullptr);
}
