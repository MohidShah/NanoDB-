/*
 * AVLTree.cpp  —  AVL self-balancing BST implementation.
 *
 * Rotation logic summary:
 *
 *   LL case  (balance > 1, left child left-heavy or balanced):
 *             rotateRight(node)
 *
 *   RR case  (balance < -1, right child right-heavy or balanced):
 *             rotateLeft(node)
 *
 *   LR case  (balance > 1, left child right-heavy):
 *             rotateLeft(node->left), then rotateRight(node)
 *
 *   RL case  (balance < -1, right child left-heavy):
 *             rotateRight(node->right), then rotateLeft(node)
 */
#include "AVLTree.h"
#include <stdio.h>   // printf

// ── Constructor / Destructor ──────────────────────────────────────────────────
AVLTree::AVLTree() : root(0), nodeCount(0) {}

AVLTree::~AVLTree() {
    freeRec(root);
}

void AVLTree::freeRec(AVLNode* n) {
    if (!n) return;
    freeRec(n->left);
    freeRec(n->right);
    delete n;
}

// ── Height update ─────────────────────────────────────────────────────────────
void AVLTree::updateHeight(AVLNode* n) {
    int lh = heightOf(n->left);
    int rh = heightOf(n->right);
    n->height = 1 + (lh > rh ? lh : rh);
}

// ── Rotations ─────────────────────────────────────────────────────────────────
/*
 *  rotateRight(y):          rotateLeft(x):
 *
 *       y                        x
 *      / \                      / \
 *     x   C       ->           A   y
 *    / \                          / \
 *   A   B                        B   C
 *
 *  x becomes new root (rotateRight); y becomes new root (rotateLeft).
 */

AVLNode* AVLTree::rotateRight(AVLNode* y) {
    AVLNode* x = y->left;
    AVLNode* B = x->right;
    x->right = y;
    y->left  = B;
    updateHeight(y);
    updateHeight(x);
    return x;   // new subtree root
}

AVLNode* AVLTree::rotateLeft(AVLNode* x) {
    AVLNode* y = x->right;
    AVLNode* B = y->left;
    y->left  = x;
    x->right = B;
    updateHeight(x);
    updateHeight(y);
    return y;   // new subtree root
}

// ── Rebalance ─────────────────────────────────────────────────────────────────
AVLNode* AVLTree::rebalance(AVLNode* n) {
    updateHeight(n);
    int bf = balanceFactor(n);

    // LL: left-heavy, left child is also left-heavy or balanced
    if (bf > 1 && balanceFactor(n->left) >= 0)
        return rotateRight(n);

    // LR: left-heavy, left child is right-heavy
    if (bf > 1 && balanceFactor(n->left) < 0) {
        n->left = rotateLeft(n->left);
        return rotateRight(n);
    }

    // RR: right-heavy, right child is also right-heavy or balanced
    if (bf < -1 && balanceFactor(n->right) <= 0)
        return rotateLeft(n);

    // RL: right-heavy, right child is left-heavy
    if (bf < -1 && balanceFactor(n->right) > 0) {
        n->right = rotateRight(n->right);
        return rotateLeft(n);
    }

    return n;   // already balanced
}

// ── insert() ──────────────────────────────────────────────────────────────────
AVLNode* AVLTree::insertRec(AVLNode* n, double key, RID rid) {
    if (!n) {
        nodeCount++;
        return new AVLNode(key, rid);
    }
    if (key < n->key)       n->left  = insertRec(n->left,  key, rid);
    else if (key > n->key)  n->right = insertRec(n->right, key, rid);
    else                    n->rid   = rid;   // duplicate key: update RID

    return rebalance(n);
}

void AVLTree::insert(double key, int pageId, int slotId) {
    root = insertRec(root, key, RID(pageId, slotId));
}

// ── search() ─────────────────────────────────────────────────────────────────
RID AVLTree::search(double key) const {
    AVLNode* cur = root;
    while (cur) {
        if (key < cur->key)      cur = cur->left;
        else if (key > cur->key) cur = cur->right;
        else                     return cur->rid;
    }
    return RID();   // not found — returns {-1, -1}
}

// ── rangeSearch() ─────────────────────────────────────────────────────────────
// In-order traversal: left subtree → root → right subtree.
// Prunes branches outside [lo, hi] to avoid unnecessary traversal.
void AVLTree::rangeRec(AVLNode* n, double lo, double hi,
                       RIDCallback cb, void* ctx) const {
    if (!n) return;
    if (n->key > lo) rangeRec(n->left,  lo, hi, cb, ctx);  // may have smaller keys
    if (n->key >= lo && n->key <= hi) cb(n->rid, ctx);
    if (n->key < hi) rangeRec(n->right, lo, hi, cb, ctx);  // may have larger keys
}

void AVLTree::rangeSearch(double lo, double hi, RIDCallback cb, void* ctx) const {
    rangeRec(root, lo, hi, cb, ctx);
}

// ── remove() ─────────────────────────────────────────────────────────────────
AVLNode* AVLTree::minNode(AVLNode* n) const {
    while (n->left) n = n->left;
    return n;
}

AVLNode* AVLTree::removeRec(AVLNode* n, double key) {
    if (!n) return 0;

    if (key < n->key) {
        n->left  = removeRec(n->left,  key);
    } else if (key > n->key) {
        n->right = removeRec(n->right, key);
    } else {
        // Found the node to delete
        if (!n->left || !n->right) {
            // One or zero children: replace with the surviving child
            AVLNode* child = n->left ? n->left : n->right;
            delete n;
            nodeCount--;
            return child;
        }
        // Two children: replace with in-order successor (min of right subtree)
        AVLNode* successor = minNode(n->right);
        n->key = successor->key;
        n->rid = successor->rid;
        n->right = removeRec(n->right, successor->key);
    }
    return rebalance(n);
}

void AVLTree::remove(double key) {
    root = removeRec(root, key);
}

// ── dump() ────────────────────────────────────────────────────────────────────
void AVLTree::dumpRec(AVLNode* n, int depth) const {
    if (!n) return;
    dumpRec(n->right, depth + 1);
    for (int i = 0; i < depth * 3; i++) printf(" ");
    printf("%.2f [p%d,s%d] h=%d\n", n->key, n->rid.pageId, n->rid.slotId, n->height);
    dumpRec(n->left, depth + 1);
}

void AVLTree::dump() const {
    printf("[AVLTree] %d nodes, tree height=%d\n", nodeCount, height());
    dumpRec(root, 0);
}
