/*
 * PriorityQueue.h  —  Binary Max-Heap for NanoDB query scheduling.
 *
 * ── Purpose ───────────────────────────────────────────────────────────────────
 *   When multiple SQL statements arrive simultaneously, admin queries must
 *   bypass the regular queue and execute first.  A max-heap naturally puts
 *   the highest-priority item at the root, available in O(1).
 *
 * ── Data structure: Binary Max-Heap ──────────────────────────────────────────
 *   Stored in a fixed-size flat array (no pointers, no STL).
 *   The heap property: heap[parent].priority >= heap[child].priority.
 *
 *   Array-to-tree mapping (0-indexed):
 *     parent(i)      = (i - 1) / 2
 *     leftChild(i)   = 2 * i + 1
 *     rightChild(i)  = 2 * i + 2
 *
 *   This layout gives cache-friendly memory access — every level of the
 *   tree is contiguous in memory.
 *
 * ── Complexity ────────────────────────────────────────────────────────────────
 *   insert()      → O(log N)  — sift-up restores heap property
 *   extractMax()  → O(log N)  — sift-down restores heap property
 *   peek()        → O(1)      — root is always the maximum
 *   size/isEmpty  → O(1)
 *
 *
 */
#ifndef PRIORITY_QUEUE_H
#define PRIORITY_QUEUE_H

#include "../Constants.h"   // PRIORITY_ADMIN, PRIORITY_USER
#include "../Logger.h"
#include <string.h>         // strncpy
#include <stdio.h>          // printf

static const int MAX_HEAP_SIZE  = 256;   // max simultaneous queued queries
static const int MAX_SQL_LENGTH = 512;   // max SQL string in a task

// ── QueryTask ─────────────────────────────────────────────────────────────────
// One unit of work scheduled for execution.
struct QueryTask {
    int  priority;              // higher = execute sooner
    int  taskId;                // monotonically increasing ID (for tie-breaking)
    char sql[MAX_SQL_LENGTH];   // the SQL statement

    QueryTask() : priority(0), taskId(0) { sql[0] = '\0'; }
    QueryTask(int prio, int id, const char* q)
        : priority(prio), taskId(id) {
        strncpy(sql, q, MAX_SQL_LENGTH - 1);
        sql[MAX_SQL_LENGTH - 1] = '\0';
    }
};

// ── PriorityQueue ─────────────────────────────────────────────────────────────
class PriorityQueue {
public:
    PriorityQueue();

    /**
     * insert() — Add a new query task to the heap.
     * @param prio  Priority level (PRIORITY_ADMIN=10, PRIORITY_USER=1).
     * @param sql   SQL statement string.
     * @return false if the heap is full.
     */
    bool insert(int prio, const char* sql);

    /**
     * extractMax() — Remove and return the highest-priority task.
     * Caller must check !isEmpty() first.
     */
    QueryTask extractMax();

    /** peek() — Return the highest-priority task without removing it. */
    const QueryTask& peek() const;

    bool isEmpty()  const { return count == 0; }
    int  size()     const { return count; }

    /** Print all tasks in heap-array order (for debugging). */
    void dump() const;

private:
    QueryTask heap[MAX_HEAP_SIZE];  // flat array storing the heap
    int       count;                // number of valid entries
    int       nextId;               // monotonically increasing task ID

    /**
     * siftUp()   — Restore heap property after inserting at index i.
     * Walks up the tree, swapping with parent while child > parent.
     */
    void siftUp(int i);

    /**
     * siftDown() — Restore heap property after removing root.
     * Walks down the tree, swapping with the larger child until in order.
     */
    void siftDown(int i);

    /** Swap two heap entries. */
    void swap(int a, int b);

    // Disallow copy
    PriorityQueue(const PriorityQueue&);
    PriorityQueue& operator=(const PriorityQueue&);
};

#endif // PRIORITY_QUEUE_H
