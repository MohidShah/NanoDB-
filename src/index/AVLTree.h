/*
 * AVLTree.h  —  Self-balancing BST index for O(log N) field lookups.
 *
 * Maps a numeric key (double) to a Record ID (pageId, slotId).
 * INT and FLOAT column values are cast to double before insertion.
 *
 * Balancing uses the AVL invariant:
 *   |height(left) - height(right)| <= 1  at every node.
 *
 * Four rotation cases restore balance after insert/delete:
 *   LL  (right rotation)          — left subtree is left-heavy
 *   RR  (left rotation)           — right subtree is right-heavy
 *   LR  (left-rotate left child,  — left subtree is right-heavy
 *         then right-rotate root)
 *   RL  (right-rotate right child,— right subtree is left-heavy
 *         then left-rotate root)
 *
 * Complexity (N = number of indexed rows):
 *   insert()      O(log N)
 *   search()      O(log N)
 *   rangeSearch() O(log N + K)  where K = number of matching rows
 *   remove()      O(log N)
 */
#ifndef AVL_TREE_H
#define AVL_TREE_H

#include <stdio.h>   // printf

// ── Record Identifier ─────────────────────────────────────────────────────────
// Locates a row within the buffer pool: fetch page `pageId`, read slot `slotId`.
struct RID {
    int pageId;
    int slotId;
    RID() : pageId(-1), slotId(-1) {}
    RID(int p, int s) : pageId(p), slotId(s) {}
    bool valid() const { return pageId >= 0 && slotId >= 0; }
};

// ── AVL Tree node ─────────────────────────────────────────────────────────────
struct AVLNode {
    double   key;     // indexed value (INT/FLOAT cast to double)
    RID      rid;     // pointer to the row on disk
    int      height;  // height of subtree rooted here (leaf = 1)
    AVLNode* left;
    AVLNode* right;

    AVLNode(double k, RID r)
        : key(k), rid(r), height(1), left(0), right(0) {}
};

// ── Callback for range search results ─────────────────────────────────────────
typedef void (*RIDCallback)(RID rid, void* ctx);

// ── AVLTree ───────────────────────────────────────────────────────────────────
class AVLTree {
public:
    AVLTree();
    ~AVLTree();   // frees all nodes (post-order delete)

    /**
     * insert() — Index a row's field value.
     * If the key already exists the RID is updated (last-write wins).
     */
    void insert(double key, int pageId, int slotId);

    /**
     * search() — Point lookup. Returns invalid RID ({-1,-1}) if not found.
     */
    RID  search(double key) const;

    /**
     * rangeSearch() — In-order traversal; calls cb(rid, ctx) for every
     * row whose key satisfies lo <= key <= hi.
     */
    void rangeSearch(double lo, double hi, RIDCallback cb, void* ctx) const;

    /**
     * remove() — Delete the node with the given key.
     * Uses in-order successor (minimum of right subtree) for node replacement.
     */
    void remove(double key);

    int  size()    const { return nodeCount; }
    int  height()  const { return nodeCount ? heightOf(root) : 0; }

    /** In-order dump showing key, pageId, slotId, and node height. */
    void dump() const;

private:
    AVLNode* root;
    int      nodeCount;

    // ── Height helpers ────────────────────────────────────────────────────────
    int  heightOf(AVLNode* n)       const { return n ? n->height : 0; }
    int  balanceFactor(AVLNode* n)  const { return heightOf(n->left) - heightOf(n->right); }
    void updateHeight(AVLNode* n);

    // ── The four rotations ────────────────────────────────────────────────────
    AVLNode* rotateRight(AVLNode* y);   // LL case
    AVLNode* rotateLeft(AVLNode* x);    // RR case

    // ── Rebalance after structural change ─────────────────────────────────────
    AVLNode* rebalance(AVLNode* n);

    // ── Recursive helpers ─────────────────────────────────────────────────────
    AVLNode* insertRec(AVLNode* n, double key, RID rid);
    AVLNode* removeRec(AVLNode* n, double key);
    AVLNode* minNode(AVLNode* n) const;   // leftmost node in subtree
    void     rangeRec(AVLNode* n, double lo, double hi,
                      RIDCallback cb, void* ctx) const;
    void     freeRec(AVLNode* n);         // post-order delete
    void     dumpRec(AVLNode* n, int depth) const;

    // Disallow copy
    AVLTree(const AVLTree&);
    AVLTree& operator=(const AVLTree&);
};

#endif // AVL_TREE_H
