/*
 * KruskalMST.cpp  —  Kruskal's MST implementation.
 */
#include "KruskalMST.h"
#include <stdio.h>

// ── sortEdges() ───────────────────────────────────────────────────────────────
// Selection sort: find the minimum-weight edge, move it to the front,
// repeat for remaining elements. O(E^2) — fine for E <= 64.
void KruskalMST::sortEdges(GraphEdge* arr, int count) {
    for (int i = 0; i < count - 1; i++) {
        int    minIdx = i;
        double minW   = arr[i].weight;
        for (int j = i + 1; j < count; j++) {
            if (arr[j].weight < minW) {
                minW   = arr[j].weight;
                minIdx = j;
            }
        }
        if (minIdx != i) {
            GraphEdge tmp = arr[i];
            arr[i]        = arr[minIdx];
            arr[minIdx]   = tmp;
        }
    }
}

// ── run() ─────────────────────────────────────────────────────────────────────
int KruskalMST::run(const Graph& g, JoinPlan& plan) {
    plan.joinCount = 0;

    if (g.nodeCount <= 1 || g.edgeCount == 0) return 0;

    // Copy edges so we can sort without mutating the graph.
    GraphEdge sorted[MAX_GRAPH_EDGES];
    for (int i = 0; i < g.edgeCount; i++) sorted[i] = g.edges[i];
    sortEdges(sorted, g.edgeCount);

    // Initialise Union-Find with one component per table node.
    UnionFind uf(g.nodeCount);

    int target = g.nodeCount - 1;   // MST for N nodes needs exactly N-1 edges

    for (int i = 0; i < g.edgeCount && plan.joinCount < target; i++) {
        const GraphEdge& e = sorted[i];
        // Accept edge only if it connects two different components (no cycle).
        if (uf.unite(e.nodeA, e.nodeB)) {
            plan.joins[plan.joinCount++] = e;
        }
        // If unite() returned false → same component → would create cycle → skip.
    }

    return plan.joinCount;
}

// ── printPlan() ───────────────────────────────────────────────────────────────
void KruskalMST::printPlan(const Graph& g, const JoinPlan& plan) {
    printf("[Optimizer] Join plan (%d steps):\n", plan.joinCount);
    for (int i = 0; i < plan.joinCount; i++) {
        const GraphEdge& e = plan.joins[i];
        printf("  Step %d: JOIN %s.%s = %s.%s  (est. cost %.0f rows)\n",
               i + 1,
               g.nodes[e.nodeA].tableName, e.colA,
               g.nodes[e.nodeB].tableName, e.colB,
               e.weight);
    }
}
