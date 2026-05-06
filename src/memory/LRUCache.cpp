/*
 * LRUCache.cpp
 * ------------
 * O(1) LRU Cache — implementation.
 *
 * DLL invariant maintained throughout:
 *   head.next  →  most recently used page
 *   tail.prev  →  least recently used page  (eviction target)
 *
 * Every get() or put() moves the referenced node to head.next.
 * Every evict() removes tail.prev and writes it to disk if dirty.
 *
 * Complexity proof:
 *   get()      → htFind() O(1) + dllRemove() O(1) + dllInsertFront() O(1) =
 * O(1) put()      → evictLRU() O(1) + new PageNode O(1) + htInsert O(1) = O(1)
 *   evictLRU() → dllRemove() O(1) + htRemove() O(1) + writeToDisk O(1) = O(1)
 */
#include "LRUCache.h"
#include <cstdlib> // NULL
#include <cstring> // memset, strcpy

// ── Constructor
// ───────────────────────────────────────────────────────────────
LRUCache::LRUCache(int cap, const char *dbFile)
    : size(0), capacity(cap), evictionCount(0) {
  // Create dummy sentinel nodes so we never need to handle null head/tail.
  // Sentinels simplify every DLL insert/remove to the same pointer dance.
  head = new PageNode(-1, nullptr); // MRU sentinel
  tail = new PageNode(-1, nullptr); // LRU sentinel
  head->next = tail;
  tail->prev = head;

  // Zero-initialise the hash table bucket array.
  for (int i = 0; i < HT_CAPACITY; i++) {
    buckets[i] = nullptr;
  }

  // Store the database file path (C-string copy; no std::string).
  // strncpy guarantees we stay within dbFilePath[256].
  strncpy(dbFilePath, dbFile, 255);
  dbFilePath[255] = '\0'; // ensure null termination

  Logger::log("LRUCache initialized: capacity=%d pages, db=%s", cap, dbFile);
}

// ── Destructor
// ────────────────────────────────────────────────────────────────
LRUCache::~LRUCache() {
  // 1. Flush all dirty pages to disk before freeing.
  flushAll();

  // 2. Walk the DLL from head to tail and free every real node.
  PageNode *cur = head->next;
  while (cur != tail) {
    PageNode *next = cur->next;
    // The Page object was heap-allocated; free it.
    delete cur->page;
    delete cur;
    cur = next;
  }

  // 3. Free sentinel nodes.
  delete head;
  delete tail;

  // 4. Free every HashEntry in the hash table.
  for (int i = 0; i < HT_CAPACITY; i++) {
    HashEntry *e = buckets[i];
    while (e) {
      HashEntry *next = e->next;
      delete e;
      e = next;
    }
    buckets[i] = nullptr;
  }
}

// ── Public: get
// ───────────────────────────────────────────────────────────────
Page *LRUCache::get(int pageId) {
  PageNode *node = htFind(pageId);
  if (!node)
    return nullptr; // cache miss

  // Cache hit — promote to MRU head (O(1) DLL operations).
  dllRemove(node);
  dllInsertFront(node);
  return node->page;
}

// ── Public: put
// ───────────────────────────────────────────────────────────────
void LRUCache::put(Page *page) {
  // If already in cache, just promote it.
  PageNode *existing = htFind(page->pageId);
  if (existing) {
    dllRemove(existing);
    dllInsertFront(existing);
    return;
  }

  // If at capacity, evict the LRU tail first.
  if (size >= capacity) {
    evictLRU();
  }

  // Create a new DLL node and insert at MRU head.
  PageNode *node = new PageNode(page->pageId, page);
  dllInsertFront(node);
  htInsert(page->pageId, node);
  size++;
}

// ── Public: flush
// ─────────────────────────────────────────────────────────────
void LRUCache::flush(int pageId) {
  PageNode *node = htFind(pageId);
  if (node && node->page && node->page->isDirty) {
    writeToDisk(node->page);
    node->page->isDirty = false;
  }
}

// ── Public: flushAll
// ──────────────────────────────────────────────────────────
void LRUCache::flushAll() {
  PageNode *cur = head->next;
  while (cur != tail) {
    if (cur->page && cur->page->isDirty) {
      writeToDisk(cur->page);
      cur->page->isDirty = false;
    }
    cur = cur->next;
  }
  Logger::log("LRUCache: all dirty pages flushed to disk.");
}

// ── Private: hash
// ─────────────────────────────────────────────────────────────
int LRUCache::hash(int pageId) const {
  // Simple modulo hash. HT_CAPACITY is a power of 2, so we can also use
  // bitwise AND: pageId & (HT_CAPACITY - 1). Both are O(1).
  // We take abs() because pageId is always non-negative, but be safe.
  int idx = pageId % HT_CAPACITY;
  return (idx < 0) ? idx + HT_CAPACITY : idx;
}

// ── Private: htInsert ────────────────────────────────────────────────────────
void LRUCache::htInsert(int pageId, PageNode *node) {
  int idx = hash(pageId);
  // Prepend to the chain at bucket[idx] (O(1)).
  HashEntry *entry = new HashEntry(pageId, node);
  entry->next = buckets[idx];
  buckets[idx] = entry;
}

// ── Private: htFind ──────────────────────────────────────────────────────────
PageNode *LRUCache::htFind(int pageId) const {
  int idx = hash(pageId);
  HashEntry *e = buckets[idx];
  // Walk the chain (O(1) amortised with a good hash and low load factor).
  while (e) {
    if (e->pageId == pageId)
      return e->node;
    e = e->next;
  }
  return nullptr;
}

// ── Private: htRemove ────────────────────────────────────────────────────────
void LRUCache::htRemove(int pageId) {
  int idx = hash(pageId);
  HashEntry *prev = nullptr;
  HashEntry *e = buckets[idx];
  while (e) {
    if (e->pageId == pageId) {
      // Unlink e from the chain.
      if (prev)
        prev->next = e->next;
      else
        buckets[idx] = e->next;
      delete e;
      return;
    }
    prev = e;
    e = e->next;
  }
}

// ── Private: dllRemove ───────────────────────────────────────────────────────
void LRUCache::dllRemove(PageNode *node) {
  // O(1) because we have both prev and next pointers.
  // Bypass `node` by wiring its neighbours together.
  node->prev->next = node->next;
  node->next->prev = node->prev;
  node->prev = nullptr;
  node->next = nullptr;
}

// ── Private: dllInsertFront
// ───────────────────────────────────────────────────
void LRUCache::dllInsertFront(PageNode *node) {
  // Insert node between `head` (MRU sentinel) and `head->next`.
  node->next = head->next;
  node->prev = head;
  head->next->prev = node;
  head->next = node;
}

// ── Private: evictLRU ────────────────────────────────────────────────────────
void LRUCache::evictLRU() {
  // The LRU target is tail->prev (the node just before the tail sentinel).
  PageNode *victim = tail->prev;
  if (victim == head)
    return; // cache is empty (shouldn't happen)

  // Do not evict a pinned page (it's actively being read/written).
  if (victim->page->isPinned()) {
    Logger::error("LRUCache: LRU page %d is pinned — cannot evict!",
                  victim->pageId);
    return;
  }

  // Flush if dirty — write-back policy.
  if (victim->page->isDirty) {
    writeToDisk(victim->page);
  }

  Logger::log("Page %d evicted via LRU, written to disk.", victim->pageId);

  // Remove from DLL and hash table.
  dllRemove(victim);
  htRemove(victim->pageId);

  // Free the Page object and the DLL node.
  delete victim->page;
  delete victim;

  size--;
  evictionCount++;
}

// ── Private: writeToDisk ─────────────────────────────────────────────────────
void LRUCache::writeToDisk(Page *page) {
  FILE *f = fopen(dbFilePath, "r+b"); // open for read+write; do not truncate
  if (!f) {
    // File may not exist yet on first write — create it.
    f = fopen(dbFilePath, "w+b");
  }
  if (!f) {
    Logger::error("Cannot open db file for write: %s", dbFilePath);
    return;
  }
  // Seek to the correct byte offset for this page.
  // Offset = pageId * PAGE_SIZE (each page occupies exactly PAGE_SIZE bytes).
  long offset = static_cast<long>(page->pageId) * PAGE_SIZE;
  fseek(f, offset, SEEK_SET);
  fwrite(page->data, 1, PAGE_SIZE, f);
  fflush(f);
  fclose(f);
}

// ── Private: readFromDisk ────────────────────────────────────────────────────
void LRUCache::readFromDisk(Page *page) {
  FILE *f = fopen(dbFilePath, "rb");
  if (!f) {
    // File doesn't exist yet → page is a fresh blank page, already zero-filled.
    return;
  }
  long offset = static_cast<long>(page->pageId) * PAGE_SIZE;
  fseek(f, offset, SEEK_SET);
  fread(page->data, 1, PAGE_SIZE, f);
  fclose(f);
}
