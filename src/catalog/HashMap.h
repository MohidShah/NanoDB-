/*
 * HashMap.h
 * ---------
 * A generic open-chaining hash map built from scratch.
 * No STL containers are used anywhere in this file.
 *
 * ── Design ───────────────────────────────────────────────────────────────────
 *   Key   : const char*  (C-string, copied internally — caller does not need
 *                         to keep the string alive after insertion)
 *   Value : void*        (caller casts to/from their concrete type)
 *
 *   The map is a fixed-size array of bucket heads.  Each bucket is a
 *   singly-linked list of Entry nodes (chaining collision resolution).
 *
 *   Hash function: FNV-1a (Fowler-Noll-Vo variant 1a)
 *     - Chosen because it distributes C-string keys extremely evenly.
 *     - Trivially implementable without any library dependency.
 *     - O(len(key)) time, which is effectively O(1) for bounded key lengths.
 *
 * ── Complexity ────────────────────────────────────────────────────────────────
 *   put()    → O(1) amortised  (O(k) in worst-case collision chain, tiny k)
 *   get()    → O(1) amortised
 *   remove() → O(1) amortised
 *
 * ── Memory ────────────────────────────────────────────────────────────────────
 *   Every Entry is heap-allocated (new / delete).
 *   The key string is deep-copied (strncpy into a fixed char[128] buffer).
 *   The caller owns the value pointer — HashMap never deletes it.
 *
 * Viva Q: "Why FNV-1a over djb2 or std::hash?"
 *   FNV-1a mixes each byte into the hash before XOR-ing, giving better
 *   avalanche for short strings.  djb2 shifts before adding, which can
 *   cluster similar keys.  std::hash is STL — forbidden by the spec.
 *
 * Viva Q: "What happens on hash collision?"
 *   Both entries sit in the same bucket's linked list.  get() walks the
 *   list comparing keys with strcmp until it finds an exact match.
 *   In the worst case (all keys collide) this degrades to O(N) — but with
 *   HM_CAPACITY = 256 and at most ~30 tables, the expected chain length
 *   is < 1, so O(1) is the practical reality.
 */
#ifndef HASH_MAP_H
#define HASH_MAP_H

#include <string.h>   // strncpy, strcmp
#include <stdio.h>    // fprintf (for debug dump)
#include <stdlib.h>   // NULL

// Maximum length of a key stored in this map.
static const int HM_MAX_KEY = 128;
// Number of buckets — power of 2 for fast modulo via bitmask.
static const int HM_CAPACITY = 256;

// ── Entry node (singly-linked list per bucket) ─────────────────────────────────
struct HashMapEntry {
    char           key[HM_MAX_KEY];  // deep-copied key string
    void*          value;            // caller-owned value pointer
    HashMapEntry*  next;             // next entry in this bucket's chain

    HashMapEntry(const char* k, void* v)
        : value(v), next(0)
    {
        strncpy(key, k, HM_MAX_KEY - 1);
        key[HM_MAX_KEY - 1] = '\0';
    }
};

// ── HashMap ───────────────────────────────────────────────────────────────────
class HashMap {
public:
    HashMap();
    ~HashMap();   // frees all Entry nodes (does NOT free values)

    /**
     * put() — Insert or overwrite a key-value pair.
     * The key is deep-copied.  The value pointer is stored as-is.
     */
    void  put(const char* key, void* value);

    /**
     * get() — Look up a key.
     * Returns the stored void* or nullptr if not found.
     */
    void* get(const char* key) const;

    /**
     * remove() — Remove the entry for `key`.
     * Returns true if the key was found and removed.
     * Does NOT free the value.
     */
    bool  remove(const char* key);

    /** Returns true if the key exists in the map. */
    bool  contains(const char* key) const;

    /** Number of keys currently stored. */
    int   size() const { return count; }

    /**
     * forEach() — Iterate over every entry.
     * Calls fn(key, value, ctx) for each pair.
     * Order is unspecified (bucket order).
     */
    typedef void (*ForEachFn)(const char* key, void* value, void* ctx);
    void  forEach(ForEachFn fn, void* ctx) const;

    /** Debug dump — prints all key→value ptr pairs to stdout. */
    void  dump() const;

private:
    HashMapEntry* buckets[HM_CAPACITY];  // fixed array of bucket heads
    int           count;                 // total entries stored

    /**
     * FNV-1a hash function.
     * Reference: http://www.isthe.com/chongo/tech/comp/fnv/
     * Prime and offset basis are fixed constants for 32-bit FNV-1a.
     */
    unsigned int fnv1a(const char* key) const;

    /** Maps a raw hash to a bucket index in [0, HM_CAPACITY). */
    int bucketFor(const char* key) const;

    // Disallow copy
    HashMap(const HashMap&);
    HashMap& operator=(const HashMap&);
};

#endif // HASH_MAP_H
