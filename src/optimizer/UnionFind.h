/*
 * UnionFind.h  —  Disjoint Set Union with path compression + union by rank.
 *
 * Used by Kruskal's algorithm to detect whether adding a join edge
 * would create a cycle in the growing MST.
 *
 * Path compression: find() flattens the tree on every call so future
 * find() calls on the same element run in near-O(1).
 *
 * Union by rank: always attach the shorter tree under the taller one,
 * keeping tree height at O(log N) even without path compression.
 *
 * Combined, operations run in amortised O(α(N)) ≈ O(1).
 */
#ifndef UNION_FIND_H
#define UNION_FIND_H

static const int UF_MAX = 32;   // max elements (tables in a query)

class UnionFind {
public:
    /**
     * @param n  Number of elements (0 .. n-1), each its own component.
     */
    explicit UnionFind(int n);

    /**
     * find() — Return the representative (root) of element x.
     * Applies path compression: every node on the path to root
     * is redirected directly to the root.
     */
    int find(int x);

    /**
     * unite() — Merge the components of x and y.
     * Returns false if x and y are already in the same component
     * (i.e., adding this edge would create a cycle — skip it).
     * Uses union by rank to keep trees shallow.
     */
    bool unite(int x, int y);

    /** True if x and y share the same component (connected). */
    bool connected(int x, int y);

    /** Reset to n independent components. */
    void reset(int n);

private:
    int parent[UF_MAX];
    int rank_[UF_MAX];   // underscore avoids clash with POSIX rank()
    int sz;
};

#endif // UNION_FIND_H
