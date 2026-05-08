/*
 * Graph.cpp
 */
#include "Graph.h"
#include <string.h>
#include <stdio.h>

Graph::Graph() : nodeCount(0), edgeCount(0) {}

void Graph::reset() {
    nodeCount = 0;
    edgeCount = 0;
}

int Graph::indexOf(const char* tableName) const {
    for (int i = 0; i < nodeCount; i++) {
        if (strcmp(nodes[i].tableName, tableName) == 0) return i;
    }
    return -1;
}

int Graph::addNode(const char* tableName, int rowCount) {
    if (indexOf(tableName) >= 0) return -1;  // already present
    if (nodeCount >= MAX_GRAPH_NODES) return -1;

    int i = nodeCount++;
    strncpy(nodes[i].tableName, tableName, 63);
    nodes[i].tableName[63] = '\0';
    nodes[i].rowCount = rowCount;
    return i;
}

bool Graph::addEdge(const char* tableA, const char* colA,
                    const char* tableB, const char* colB) {
    int ia = indexOf(tableA);
    int ib = indexOf(tableB);
    if (ia < 0 || ib < 0 || edgeCount >= MAX_GRAPH_EDGES) return false;

    GraphEdge& e = edges[edgeCount++];
    e.nodeA  = ia;
    e.nodeB  = ib;
    // Weight = product of row counts — smaller product = cheaper join
    e.weight = (double)nodes[ia].rowCount * (double)nodes[ib].rowCount;
    strncpy(e.colA, colA, 63);  e.colA[63] = '\0';
    strncpy(e.colB, colB, 63);  e.colB[63] = '\0';
    return true;
}

void Graph::dump() const {
    printf("[Graph] %d nodes, %d edges:\n", nodeCount, edgeCount);
    for (int i = 0; i < nodeCount; i++)
        printf("  node[%d] %s  rows=%d\n", i, nodes[i].tableName, nodes[i].rowCount);
    for (int i = 0; i < edgeCount; i++)
        printf("  edge[%d] %s.%s = %s.%s  weight=%.0f\n", i,
               nodes[edges[i].nodeA].tableName, edges[i].colA,
               nodes[edges[i].nodeB].tableName, edges[i].colB,
               edges[i].weight);
}
