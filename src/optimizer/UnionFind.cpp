/*
 * UnionFind.cpp
 */
#include "UnionFind.h"

UnionFind::UnionFind(int n) {
    reset(n);
}

void UnionFind::reset(int n) {
    sz = n;
    for (int i = 0; i < n; i++) {
        parent[i] = i;   // each element is its own root
        rank_[i]  = 0;
    }
}

// Path compression: redirect every visited node directly to the root.
int UnionFind::find(int x) {
    if (parent[x] != x)
        parent[x] = find(parent[x]);   // recursive compression
    return parent[x];
}

// Union by rank: attach smaller-rank tree under larger-rank root.
bool UnionFind::unite(int x, int y) {
    int rx = find(x);
    int ry = find(y);
    if (rx == ry) return false;   // already in same set — cycle detected

    if (rank_[rx] < rank_[ry])       parent[rx] = ry;
    else if (rank_[rx] > rank_[ry])  parent[ry] = rx;
    else { parent[ry] = rx; rank_[rx]++; }

    return true;
}

bool UnionFind::connected(int x, int y) {
    return find(x) == find(y);
}
