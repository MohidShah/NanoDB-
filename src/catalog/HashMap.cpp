/*
 * HashMap.cpp
 * -----------
 * Implementation of the FNV-1a open-chaining HashMap.
 *
 * Memory contract:
 *   - put()    : allocates one HashMapEntry per new key (new)
 *   - remove() : frees the removed entry (delete)
 *   - ~HashMap : frees all remaining entries (delete in destructor)
 *   - Values (void*) are NEVER freed here — the caller owns them.
 */
#include "HashMap.h"
#include <stdio.h>   // printf
#include <string.h>  // strcmp, strncpy
#include <stdlib.h>  // NULL

// ── FNV-1a constants (32-bit) ─────────────────────────────────────────────────
static const unsigned int FNV_PRIME  = 16777619u;
static const unsigned int FNV_OFFSET = 2166136261u;

// ── Constructor / Destructor ──────────────────────────────────────────────────
HashMap::HashMap() : count(0) {
    // Zero-initialise all bucket pointers.
    for (int i = 0; i < HM_CAPACITY; i++) {
        buckets[i] = 0;
    }
}

HashMap::~HashMap() {
    // Walk every bucket chain and free every node.
    // Values are NOT freed — the caller owns them.
    for (int i = 0; i < HM_CAPACITY; i++) {
        HashMapEntry* cur = buckets[i];
        while (cur) {
            HashMapEntry* next = cur->next;
            delete cur;   // frees the node (key buffer is inside the node)
            cur = next;
        }
        buckets[i] = 0;
    }
}

// ── Hash function ─────────────────────────────────────────────────────────────
unsigned int HashMap::fnv1a(const char* key) const {
    unsigned int hash = FNV_OFFSET;
    // Process each byte: XOR then multiply.
    // The XOR-before-multiply order is what distinguishes FNV-1a from FNV-1.
    for (const char* p = key; *p != '\0'; ++p) {
        hash ^= static_cast<unsigned char>(*p);
        hash *= FNV_PRIME;
    }
    return hash;
}

int HashMap::bucketFor(const char* key) const {
    // Use bitwise AND with (capacity - 1) as an efficient modulo.
    // Only valid when HM_CAPACITY is a power of 2.
    return static_cast<int>(fnv1a(key) & (HM_CAPACITY - 1));
}

// ── put() ─────────────────────────────────────────────────────────────────────
void HashMap::put(const char* key, void* value) {
    int b = bucketFor(key);

    // Search existing chain for key — update if found (overwrite semantics).
    HashMapEntry* cur = buckets[b];
    while (cur) {
        if (strcmp(cur->key, key) == 0) {
            cur->value = value;   // overwrite existing value
            return;
        }
        cur = cur->next;
    }

    // Key not found — prepend a new node to the bucket chain.
    // Prepend (O(1)) is preferred over append; order is irrelevant for a map.
    HashMapEntry* entry = new HashMapEntry(key, value);
    entry->next  = buckets[b];
    buckets[b]   = entry;
    count++;
}

// ── get() ─────────────────────────────────────────────────────────────────────
void* HashMap::get(const char* key) const {
    int b = bucketFor(key);
    HashMapEntry* cur = buckets[b];
    while (cur) {
        if (strcmp(cur->key, key) == 0) return cur->value;
        cur = cur->next;
    }
    return 0;   // not found
}

// ── contains() ────────────────────────────────────────────────────────────────
bool HashMap::contains(const char* key) const {
    return get(key) != 0;
}

// ── remove() ─────────────────────────────────────────────────────────────────
bool HashMap::remove(const char* key) {
    int b = bucketFor(key);
    HashMapEntry* prev = 0;
    HashMapEntry* cur  = buckets[b];
    while (cur) {
        if (strcmp(cur->key, key) == 0) {
            // Unlink from chain
            if (prev) prev->next = cur->next;
            else      buckets[b] = cur->next;
            delete cur;
            count--;
            return true;
        }
        prev = cur;
        cur  = cur->next;
    }
    return false;   // key not found
}

// ── forEach() ─────────────────────────────────────────────────────────────────
void HashMap::forEach(ForEachFn fn, void* ctx) const {
    for (int i = 0; i < HM_CAPACITY; i++) {
        HashMapEntry* cur = buckets[i];
        while (cur) {
            fn(cur->key, cur->value, ctx);
            cur = cur->next;
        }
    }
}

// ── dump() ────────────────────────────────────────────────────────────────────
void HashMap::dump() const {
    printf("[HashMap] %d entries across %d buckets:\n", count, HM_CAPACITY);
    for (int i = 0; i < HM_CAPACITY; i++) {
        HashMapEntry* cur = buckets[i];
        if (!cur) continue;
        printf("  bucket[%d]: ", i);
        while (cur) {
            printf("\"%s\" -> %p  ", cur->key, cur->value);
            cur = cur->next;
        }
        printf("\n");
    }
}
