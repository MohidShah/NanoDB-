/*
 * Stack.h  —  Generic linked-list stack (header-only template).
 *
 * No STL containers used. Template is allowed by the spec; only
 * std::stack / std::vector / std::map etc. are forbidden.
 *
 * Viva Q: "Why linked-list instead of array?"
 *   A linked-list stack has no fixed capacity — it grows with the heap.
 *   An array stack would need a MAX_DEPTH constant; for arbitrarily
 *   nested parentheses in SQL, dynamic allocation is safer.
 *
 * Complexity: push O(1), pop O(1), peek O(1), all via pointer manipulation.
 * Memory: each node heap-allocated; destructor frees all nodes.
 */
#ifndef STACK_H
#define STACK_H

#include <stdlib.h>  // NULL

template <typename T>
class Stack {
public:
    Stack() : topNode(0), sz(0) {}

    ~Stack() {
        while (!empty()) pop();
    }

    void push(const T& val) {
        Node* n  = new Node(val);
        n->next  = topNode;
        topNode  = n;
        sz++;
    }

    // Returns value and removes from stack. Caller must ensure !empty().
    T pop() {
        T val    = topNode->data;
        Node* dead = topNode;
        topNode  = topNode->next;
        delete dead;
        sz--;
        return val;
    }

    // Peek at top without removing. Caller must ensure !empty().
    T& peek()             { return topNode->data; }
    const T& peek() const { return topNode->data; }

    bool empty() const { return topNode == 0; }
    int  size()  const { return sz; }

private:
    struct Node {
        T     data;
        Node* next;
        Node(const T& d) : data(d), next(0) {}
    };

    Node* topNode;
    int   sz;

    // Disallow copy
    Stack(const Stack&);
    Stack& operator=(const Stack&);
};

#endif // STACK_H
