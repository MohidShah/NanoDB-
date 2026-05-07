/*
 * Page.h
 * ------
 * A Page is the fundamental unit of data transfer between disk and RAM
 * in NanoDB's Buffer Pool.
 *
 * Every row stored in any table ultimately lives on one of these pages.
 * When the Buffer Pool needs a record, it loads the whole page that
 * contains it into RAM — not just the record itself.  This mirrors how
 * real RDBMS engines (PostgreSQL, InnoDB) work.
 *
 * Memory layout:
 *   +--------------------------------------------------+
 *   |  char data[PAGE_SIZE]  (4 096 raw bytes)         |
 *   +--------------------------------------------------+
 *   Byte 0..3   : page ID (stored in-header for self-identification)
 *   Byte 4..7   : number of rows currently stored
 *   Byte 8..N   : serialized row data (written by Table layer)
 *
 * Pointer arithmetic:
 *   data + offset  →  advances the char* by exactly `offset` bytes.
 *   This is defined behaviour because char has sizeof == 1.
 *
 * Ownership:
 *   Page owns the `data` buffer.  It is allocated in the constructor
 *   with `new char[PAGE_SIZE]` and released in the destructor with
 *   `delete[] data`.  Never freed anywhere else.
 *
 * Viva Q: "Why PAGE_SIZE = 4096?"
 *   4 KB matches the virtual memory page size on x86/x64 Linux and
 *   Windows.  Aligning our DB pages to OS pages avoids TLB splits
 *   when doing raw file I/O with fread/fwrite.
 */
#ifndef PAGE_H
#define PAGE_H

#include <cstring>  // memset, memcpy
#include <cstdio>   // fprintf (for debug)
#include "../Constants.h"  // PAGE_SIZE, PAGE_HEADER_SZ and all shared constants

class Page {
public:
    int    pageId;    // logical page number; disk offset = pageId * PAGE_SIZE
    char*  data;      // raw 4-KB heap block; owned by this Page object
    bool   isDirty;   // true → modified since last disk flush; must write back
    int    pinCount;  // >0 → page is actively in use; must NOT be evicted

    /**
     * Constructor — allocates the raw data block on the heap.
     * The block is zero-filled so stale bytes never appear in fresh pages.
     */
    explicit Page(int id);

    /**
     * Destructor — releases the heap-allocated data block.
     * Pairing `new char[]` with `delete[]` is mandatory; mismatching
     * with `delete` (no brackets) is undefined behaviour.
     */
    ~Page();

    // ── Data access ───────────────────────────────────────────────────────────

    /**
     * write() — Copy `size` bytes from `src` into this page at `offset`.
     * Sets isDirty = true so the Buffer Pool knows to flush on eviction.
     * Returns false if [offset, offset+size) would exceed PAGE_SIZE.
     */
    bool write(const char* src, int offset, int size);

    /**
     * read() — Copy `size` bytes from this page (at `offset`) into `dst`.
     * Returns false if [offset, offset+size) would exceed PAGE_SIZE.
     */
    bool read(char* dst, int offset, int size) const;

    /** Zero-fill the entire data block (used when re-using an evicted page). */
    void clear();

    // ── Pin/unpin protocol ────────────────────────────────────────────────────
    void pin()         { pinCount++; }
    void unpin()       { if (pinCount > 0) pinCount--; }
    bool isPinned()  const { return pinCount > 0; }

    // ── Header helpers ────────────────────────────────────────────────────────
    /** Write the page ID into bytes 0-3 of data (self-identifying pages). */
    void writeHeader();

    /** Read the page ID back from bytes 0-3 (used on deserialization). */
    int  readHeaderId() const;

    /** Store row count in bytes 4-7 of data. */
    void setRowCount(int count);

    /** Retrieve row count from bytes 4-7 of data. */
    int  getRowCount() const;

private:
    // Disallow copy and assignment — pages are unique resources.
    Page(const Page&);
    Page& operator=(const Page&);
};

#endif // PAGE_H
