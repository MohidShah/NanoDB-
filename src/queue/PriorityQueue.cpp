/*
 * PriorityQueue.cpp  —  Binary max-heap implementation.
 *
 * Heap invariant maintained at all times:
 *   heap[parent(i)].priority >= heap[i].priority  for all i > 0
 *
 * Tie-breaking: when two tasks share the same priority, the one with the
 * lower taskId (i.e., earlier arrival) executes first.  This is enforced
 * in siftUp/siftDown by the comparison:
 *   "heap[a] > heap[b]" means:
 *     priority[a] > priority[b]
 *     OR (priority[a] == priority[b] AND taskId[a] < taskId[b])
 */
#include "PriorityQueue.h"
#include <stdio.h>   // printf
#include <string.h>  // strncpy

// ── Constructor ───────────────────────────────────────────────────────────────
PriorityQueue::PriorityQueue() : count(0), nextId(0) {}

// ── Comparison helper (used by siftUp and siftDown) ───────────────────────────
// Returns true if heap[a] should be ABOVE heap[b] in a max-heap.
static bool higherPriority(const QueryTask& a, const QueryTask& b) {
    if (a.priority != b.priority) return a.priority > b.priority;
    return a.taskId < b.taskId;   // FIFO tie-break: lower ID = arrived earlier
}

// ── swap() ────────────────────────────────────────────────────────────────────
void PriorityQueue::swap(int a, int b) {
    QueryTask tmp = heap[a];
    heap[a] = heap[b];
    heap[b] = tmp;
}

// ── siftUp() ─────────────────────────────────────────────────────────────────
// After inserting at index `i`, walk up to restore heap property.
// At each step: if child has higher priority than parent → swap.
// Stops when child is in the correct position (O(log N) steps).
void PriorityQueue::siftUp(int i) {
    while (i > 0) {
        int parent = (i - 1) / 2;
        if (higherPriority(heap[i], heap[parent])) {
            swap(i, parent);
            i = parent;
        } else {
            break;
        }
    }
}

// ── siftDown() ───────────────────────────────────────────────────────────────
// After placing the last element at root (extractMax), walk down.
// At each step: swap with the larger child if child > current.
// Stops when in the correct position (O(log N) steps).
void PriorityQueue::siftDown(int i) {
    while (true) {
        int left    = 2 * i + 1;
        int right   = 2 * i + 2;
        int largest = i;

        // Check if left child has higher priority than current largest
        if (left < count && higherPriority(heap[left], heap[largest])) {
            largest = left;
        }
        // Check if right child has higher priority than current largest
        if (right < count && higherPriority(heap[right], heap[largest])) {
            largest = right;
        }

        if (largest == i) break;   // heap property satisfied

        swap(i, largest);
        i = largest;
    }
}

// ── insert() ──────────────────────────────────────────────────────────────────
bool PriorityQueue::insert(int prio, const char* sql) {
    if (count >= MAX_HEAP_SIZE) {
        Logger::error("PriorityQueue full (%d tasks). Cannot enqueue: %s", MAX_HEAP_SIZE, sql);
        return false;
    }
    heap[count] = QueryTask(prio, nextId++, sql);
    siftUp(count);
    count++;
    Logger::log("Queued [id=%d pri=%d]: %s", nextId - 1, prio, sql);
    return true;
}

// ── extractMax() ──────────────────────────────────────────────────────────────
QueryTask PriorityQueue::extractMax() {
    // Grab the root (highest-priority task).
    QueryTask top = heap[0];

    // Move the last element to the root and shrink the heap.
    count--;
    if (count > 0) {
        heap[0] = heap[count];
        siftDown(0);             // restore heap property from the root down
    }

    Logger::log("Dequeued [id=%d pri=%d]: %s", top.taskId, top.priority, top.sql);
    return top;
}

// ── peek() ────────────────────────────────────────────────────────────────────
const QueryTask& PriorityQueue::peek() const {
    return heap[0];  // root is always the maximum; caller must ensure !isEmpty()
}

// ── dump() ────────────────────────────────────────────────────────────────────
void PriorityQueue::dump() const {
    printf("[PriorityQueue] %d task(s):\n", count);
    for (int i = 0; i < count; i++) {
        printf("  heap[%2d]  pri=%-3d  id=%-3d  sql=%s\n",
               i, heap[i].priority, heap[i].taskId, heap[i].sql);
    }
}
