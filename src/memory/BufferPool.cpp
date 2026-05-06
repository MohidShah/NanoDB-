/*
 * BufferPool.cpp
 * --------------
 * Buffer Pool Manager implementation.
 *
 * Page loading strategy (fetchPage):
 *   1. Ask LRUCache::get()  →  O(1) cache lookup.
 *   2. Cache hit?  Promote to MRU, pin, return.
 *   3. Cache miss? Create a new Page object, read data from disk,
 *      insert into cache (may trigger LRU eviction), pin, return.
 *
 * The "pin" mechanism prevents an actively-used page from being
 * evicted mid-operation.  This is critical during multi-page scans
 * or joins where the same page might be accessed multiple times.
 */
#include "BufferPool.h"
#include <cstring>  // strncpy

// ── Constructor ───────────────────────────────────────────────────────────────
BufferPool::BufferPool(int maxPages, const char* path)
    : nextPageId(0)
{
    strncpy(dbFilePath, path, 255);
    dbFilePath[255] = '\0';

    // Allocate the LRU cache. The cache will handle all eviction logic.
    cache = new LRUCache(maxPages, path);

    Logger::log("BufferPool created: maxPages=%d, file=%s", maxPages, path);
}

// ── Destructor ────────────────────────────────────────────────────────────────
BufferPool::~BufferPool() {
    // Flush remaining dirty pages, then free the cache and all its nodes.
    delete cache;    // LRUCache destructor calls flushAll() + frees all nodes
    Logger::log("BufferPool destroyed. All pages flushed.");
}

// ── fetchPage ─────────────────────────────────────────────────────────────────
Page* BufferPool::fetchPage(int pageId) {
    // ── Step 1: Try the cache ──────────────────────────────────────────────
    Page* p = cache->get(pageId);

    if (p) {
        // Cache hit — promote already handled by LRUCache::get().
        p->pin();
        return p;
    }

    // ── Step 2: Cache miss — load from disk ───────────────────────────────
    // Allocate a fresh Page object; its constructor zero-fills data[].
    p = new Page(pageId);

    // Attempt to read data from the database file.
    // If the file doesn't contain this page yet, data[] stays zero-filled.
    FILE* f = fopen(dbFilePath, "rb");
    if (f) {
        long offset = static_cast<long>(pageId) * PAGE_SIZE;
        if (fseek(f, offset, SEEK_SET) == 0) {
            fread(p->data, 1, PAGE_SIZE, f);
            p->isDirty = false;  // freshly loaded; not modified yet
        }
        fclose(f);
    }

    // ── Step 3: Insert into cache (may evict LRU page) ────────────────────
    cache->put(p);   // ownership of p transfers to cache
    p->pin();
    return p;
}

// ── newPage ───────────────────────────────────────────────────────────────────
Page* BufferPool::newPage() {
    int id = nextPageId++;

    // Allocate fresh zero-filled page (no disk read needed for new pages).
    Page* p = new Page(id);
    p->writeHeader();   // embed page ID in bytes 0-3
    p->setRowCount(0);  // initialise row count to zero

    // Insert into cache.
    cache->put(p);
    p->pin();

    Logger::log("BufferPool: allocated new Page %d.", id);
    return p;
}

// ── releasePage ───────────────────────────────────────────────────────────────
void BufferPool::releasePage(int pageId) {
    Page* p = cache->get(pageId);
    if (p) {
        p->unpin();
    }
}

// ── markDirty ─────────────────────────────────────────────────────────────────
void BufferPool::markDirty(int pageId) {
    Page* p = cache->get(pageId);
    if (p) {
        p->isDirty = true;
    }
}

// ── flushAll ──────────────────────────────────────────────────────────────────
void BufferPool::flushAll() {
    cache->flushAll();
    Logger::log("BufferPool: flushAll() complete — data persisted to disk.");
}

// ── getEvictionCount ──────────────────────────────────────────────────────────
int BufferPool::getEvictionCount() const {
    return cache->getEvictionCount();
}
