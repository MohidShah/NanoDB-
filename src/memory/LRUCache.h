/*
 * LRUCache.h
 * ----------
 * Implements an O(1) Least Recently Used (LRU) cache for database pages.
 *
 * ── Why O(1)? ────────────────────────────────────────────────────────────────
 * A naive LRU using a simple array would require O(N) scans to find the
 * least-recently-used item.  By combining two structures:
 *   1. Doubly Linked List (DLL) — keeps pages ordered by recency.
 *      Head = Most Recently Used (MRU), Tail = Least Recently Used (LRU).
 *   2. Hash Table (array of chains) — maps pageId → DLL node* in O(1).
 * …every operation (get, put, evict) runs in O(1) amortised time.
 *
 * ── DLL Node layout ──────────────────────────────────────────────────────────
 *
 *   MRU [head] ←→ node ←→ node ←→ ... ←→ node [tail] LRU
 *
 *   Each node holds:
 *     - pageId    : the logical page number (cache key)
 *     - page      : pointer to the Page object (cache value)
 *     - prev/next : DLL links (raw pointers, no smart pointers)
 *
 * ── Hash table ────────────────────────────────────────────────────────────────
 *   buckets[pageId % HT_CAPACITY] → linked list of HashEntry{pageId, node*}
 *   Collision resolution: chaining (each bucket is a singly-linked list).
 *
 * Viva Q: "Why a doubly linked list instead of singly linked?"
 *   To remove an arbitrary node in O(1) we need to update BOTH its
 *   predecessor and successor.  A singly linked list can only traverse
 *   forward, so removal requires O(N) to find the predecessor.
 */
#ifndef LRU_CACHE_H
#define LRU_CACHE_H

#include "Page.h"
#include "../Logger.h"
#include <cstdio>   // fprintf for disk I/O (fopen, fread, fwrite)

// ── Doubly Linked List node ───────────────────────────────────────────────────
struct PageNode {
    int       pageId;
    Page*     page;      // owned by LRUCache; freed on eviction
    PageNode* prev;      // towards MRU head
    PageNode* next;      // towards LRU tail

    PageNode(int id, Page* p)
        : pageId(id), page(p), prev(nullptr), next(nullptr) {}
};

// ── Hash table entry (chaining) ───────────────────────────────────────────────
struct HashEntry {
    int        pageId;
    PageNode*  node;     // pointer into the DLL — enables O(1) DLL operations
    HashEntry* next;     // next entry in this bucket's chain

    HashEntry(int id, PageNode* n)
        : pageId(id), node(n), next(nullptr) {}
};

// ── LRU Cache ─────────────────────────────────────────────────────────────────
class LRUCache {
public:
    /**
     * Constructor.
     * @param cap      Maximum number of pages to hold in RAM at once.
     * @param dbFile   Path to the database file for reading/writing pages.
     */
    LRUCache(int cap, const char* dbFile);

    /**
     * Destructor — evicts (flushes) all remaining dirty pages and frees
     * every PageNode and HashEntry allocated during the cache's lifetime.
     */
    ~LRUCache();

    /**
     * get() — Fetch a page from the cache.
     * If found: moves it to the MRU head and returns its Page*.
     * If not found: returns nullptr (caller must load from disk via put()).
     * Complexity: O(1) amortised.
     */
    Page* get(int pageId);

    /**
     * put() — Insert a new page into the cache.
     * If the cache is at capacity, the LRU tail is evicted first.
     * Complexity: O(1) amortised.
     * @param page  Heap-allocated Page*; ownership transfers to the cache.
     */
    void put(Page* page);

    /**
     * flush() — Write a specific dirty page to disk without evicting it.
     */
    void flush(int pageId);

    /**
     * flushAll() — Write all dirty pages to disk (called at shutdown).
     */
    void flushAll();

    /**
     * getEvictionCount() — Returns total pages evicted since construction.
     * Used by Test Case D to count LRU misses.
     */
    int getEvictionCount() const { return evictionCount; }

    int getSize()     const { return size; }
    int getCapacity() const { return capacity; }

private:
    // ── DLL ───────────────────────────────────────────────────────────────────
    PageNode* head;   // MRU sentinel (dummy node, never holds a real page)
    PageNode* tail;   // LRU sentinel (dummy node, never holds a real page)
    int       size;
    int       capacity;

    // ── Hash table ────────────────────────────────────────────────────────────
    static const int HT_CAPACITY = 1024;  // bucket count; must be power of 2
    HashEntry* buckets[HT_CAPACITY];

    // ── Disk I/O ──────────────────────────────────────────────────────────────
    char dbFilePath[256];   // path to the .dbf database file
    int  evictionCount;     // total pages evicted (for Test Case D)

    // ── Private helpers ───────────────────────────────────────────────────────

    /** Hash function: maps pageId to a bucket index. */
    int hash(int pageId) const;

    /** Insert (pageId → node) into the hash table. */
    void htInsert(int pageId, PageNode* node);

    /** Look up a node by pageId.  Returns nullptr if not found. */
    PageNode* htFind(int pageId) const;

    /** Remove entry for pageId from the hash table. */
    void htRemove(int pageId);

    /** Unlink a node from the DLL (O(1) thanks to prev/next pointers). */
    void dllRemove(PageNode* node);

    /** Insert a node immediately after the MRU head (O(1)). */
    void dllInsertFront(PageNode* node);

    /** Evict the LRU tail page (flush if dirty, then free). */
    void evictLRU();

    /** Write a page's data to disk at the correct byte offset. */
    void writeToDisk(Page* page);

    /** Read a page's data from disk into an existing Page object. */
    void readFromDisk(Page* page);

    // Disallow copy
    LRUCache(const LRUCache&);
    LRUCache& operator=(const LRUCache&);
};

#endif // LRU_CACHE_H
