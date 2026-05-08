/*
 * KruskalMST.h  —  Kruskal's Minimum Spanning Tree for join-order optimisation.
 *
 * Algorithm:
 *   1. Copy all edges from the Graph into a local array.
 *   2. Sort edges by weight ascending (selection sort — no std::sort).
 *   3. Iterate sorted edges; use UnionFind to accept edges that do not
 *      create a cycle.  Accepted edges form the MST.
 *   4. The MST edges, in acceptance order, define the optimal join sequence:
 *      execute the cheapest (smallest cross-product) join first.
 *
 * For N tables, the MST has exactly N-1 edges (join steps).
 *
 * Complexity:
 *   Sort (selection sort): O(E^2)  — E = number of join edges (tiny, <= 64)
 *   Kruskal main loop:     O(E * α(N))  ≈ O(E)
 *   Total:                 O(E^2) dominated by sort; acceptable for E <= 64.
 */
#ifndef KRUSKAL_MST_H
#define KRUSKAL_MST_H

#include "Graph.h"
#include "UnionFind.h"
#include <stdio.h>

// ── Join plan produced by the optimizer ───────────────────────────────────────
struct JoinPlan {
    GraphEdge joins[MAX_GRAPH_NODES];  // MST edges in optimal execution order
    int       joinCount;               // number of join steps (= tables - 1)

    JoinPlan() : joinCount(0) {}
};

class KruskalMST {
public:
    /**
     * run() — Execute Kruskal's algorithm on the given graph.
     * Populates `plan` with the MST edges in ascending weight order.
     * Returns the number of edges in the MST (should be nodeCount - 1).
     */
    int run(const Graph& g, JoinPlan& plan);

    /** Print the join plan in execution order. */
    static void printPlan(const Graph& g, const JoinPlan& plan);

private:
    /** Selection sort on a copy of the edge array (ascending weight). */
    void sortEdges(GraphEdge* arr, int count);
};

#endif // KRUSKAL_MST_H
