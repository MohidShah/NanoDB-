/*
 * Graph.h  —  Join graph for the NanoDB query optimizer.
 *
 * Models a multi-table query as an undirected weighted graph:
 *   Node  = one table participating in the query.
 *   Edge  = one join condition between two tables.
 *   Weight = estimated join cost (product of both tables' row counts;
 *            smaller weight → cheaper join → preferred by MST).
 *
 * Kruskal's MST on this graph finds the join order that minimises
 * the total estimated intermediate result size.
 */
#ifndef GRAPH_H
#define GRAPH_H

#include <string.h>   // strncpy
#include <stdio.h>    // printf

static const int MAX_GRAPH_NODES = 16;   // max tables in one query
static const int MAX_GRAPH_EDGES = 64;   // max join conditions

// ── Graph node: one participating table ───────────────────────────────────────
struct GraphNode {
    char tableName[64];
    int  rowCount;          // estimated cardinality (used for weight calculation)

    GraphNode() : rowCount(0) { tableName[0] = '\0'; }
};

// ── Graph edge: one join condition ────────────────────────────────────────────
struct GraphEdge {
    int    nodeA;           // index of left-hand table in nodes[]
    int    nodeB;           // index of right-hand table in nodes[]
    double weight;          // estimated cost: rowCount[A] * rowCount[B]
    char   colA[64];        // join column on table A
    char   colB[64];        // join column on table B

    GraphEdge() : nodeA(-1), nodeB(-1), weight(0.0) {
        colA[0] = '\0';
        colB[0] = '\0';
    }
};

// ── Graph ─────────────────────────────────────────────────────────────────────
class Graph {
public:
    GraphNode nodes[MAX_GRAPH_NODES];
    GraphEdge edges[MAX_GRAPH_EDGES];
    int       nodeCount;
    int       edgeCount;

    Graph();

    /**
     * addNode() — Register a table as a graph node.
     * Returns the node index, or -1 if the table is already present.
     */
    int  addNode(const char* tableName, int rowCount);

    /**
     * addEdge() — Add a join condition between two already-registered tables.
     * Weight = rowCount[A] * rowCount[B] (cross-product cost estimate).
     * Returns false if either table name is not found.
     */
    bool addEdge(const char* tableA, const char* colA,
                 const char* tableB, const char* colB);

    /** Find a node index by table name. Returns -1 if not found. */
    int  indexOf(const char* tableName) const;

    void dump() const;

    void reset();
};

#endif // GRAPH_H
