/*
 * BufferPool.h
 * ------------
 * The Buffer Pool Manager is the public face of NanoDB's memory layer.
 *
 * All higher layers (Schema, Engine) fetch and release pages through the
 * BufferPool — they never touch the LRUCache or disk directly.
 *
 * Responsibilities:
 *   1. fetchPage(id)  — Return the Page* for `id`, loading from disk if needed.
 *   2. newPage()      — Allocate the next available page ID, return its Page*.
 *   3. releasePage(id)— Unpin a page so it becomes eligible for eviction.
 *   4. flushAll()     — Force-write all dirty pages (called before shutdown).
 *
 * The BufferPool owns exactly one LRUCache instance.
 * It does NOT own individual Page objects — the LRUCache does.
 *
 * Viva Q: "What is the difference between the Buffer Pool and the LRU Cache?"
 *   LRUCache is the policy engine (eviction, hash lookup, DLL ordering).
 *   BufferPool is the interface layer (page numbering, pin tracking, disk
 *   loading).  Separating them respects the Single Responsibility Principle.
 */
#ifndef BUFFER_POOL_H
#define BUFFER_POOL_H

#include "LRUCache.h"
#include "../Logger.h"

class BufferPool {
public:
    /**
     * Constructor.
     * @param maxPages   Number of pages that can reside in RAM simultaneously.
     * @param dbFilePath Path to the binary database file (created if absent).
     */
    BufferPool(int maxPages, const char* dbFilePath);

    /**
     * Destructor — flushes all dirty pages and destroys the LRUCache.
     */
    ~BufferPool();

    /**
     * fetchPage() — Return a pinned pointer to the requested page.
     * If the page is in cache: cache hit, return immediately.
     * If not in cache: load from disk, insert into cache (may evict LRU page).
     * The returned page is PINNED — caller must call releasePage() when done.
     */
    Page* fetchPage(int pageId);

    /**
     * newPage() — Allocate the next logical page (does NOT load from disk).
     * Returns a pinned, zero-filled Page*.
     */
    Page* newPage();

    /**
     * releasePage() — Unpin a page.
     * Unpinned pages are eligible for LRU eviction when the cache is full.
     */
    void releasePage(int pageId);

    /**
     * markDirty() — Explicitly mark a page as modified.
     * (Page::write() already sets isDirty, but this is useful when the
     *  caller edits page->data directly via pointer arithmetic.)
     */
    void markDirty(int pageId);

    /**
     * flushAll() — Flush all dirty pages to disk.
     * Called at engine shutdown and before Test Case G (persistence check).
     */
    void flushAll();

    /** Returns the total number of LRU evictions so far (Test Case D). */
    int getEvictionCount() const;

    /** Returns the next available (unallocated) page ID. */
    int getNextPageId() const { return nextPageId; }

private:
    LRUCache* cache;      // owned; freed in destructor
    int       nextPageId; // monotonically increasing page counter

    char dbFilePath[256];

    // Disallow copy
    BufferPool(const BufferPool&);
    BufferPool& operator=(const BufferPool&);
};

#endif // BUFFER_POOL_H
