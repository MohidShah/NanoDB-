/*
 * Row.cpp
 * -------
 * Implements Row — the polymorphic, heap-managed record type.
 *
 * Memory management summary:
 *   allocFields()  → new Field* per column  (called in constructor + deserialize)
 *   freeFields()   → delete each Field*, then delete[] cols  (called in destructor)
 *   clone()        → Field::clone() performs a virtual deep-copy
 *
 * Viva Q: "Why clone() instead of just copying the pointer?"
 *   Two Rows sharing the same Field* pointer would cause a double-free in
 *   their destructors.  clone() ensures each Row owns its own Field objects.
 */
#include "Row.h"
#include <cstring>  // memcpy
#include <cstdio>   // printf

// ── allocFields ───────────────────────────────────────────────────────────────
void Row::allocFields(const Schema* schema) {
    numCols = schema->numCols;
    // Allocate the pointer array. Each element is a Field*.
    cols = new Field*[numCols];
    for (int i = 0; i < numCols; i++) {
        switch (schema->columns[i].type) {
            case Field::INT:    cols[i] = new IntField();    break;
            case Field::FLOAT:  cols[i] = new FloatField();  break;
            case Field::STRING: cols[i] = new StringField(); break;
            default:            cols[i] = nullptr;           break;
        }
    }
}

// ── freeFields ────────────────────────────────────────────────────────────────
void Row::freeFields() {
    for (int i = 0; i < numCols; i++) {
        delete cols[i];    // virtual destructor dispatches correctly
        cols[i] = nullptr;
    }
    delete[] cols;         // free the pointer array itself
    cols = nullptr;
}

// ── Constructor ───────────────────────────────────────────────────────────────
Row::Row(const Schema* schema) : cols(nullptr), numCols(0) {
    allocFields(schema);
}

// ── Destructor ────────────────────────────────────────────────────────────────
Row::~Row() {
    freeFields();
}

// ── Copy constructor ──────────────────────────────────────────────────────────
Row::Row(const Row& other) : cols(nullptr), numCols(other.numCols) {
    cols = new Field*[numCols];
    for (int i = 0; i < numCols; i++) {
        cols[i] = other.cols[i]->clone();  // virtual deep copy
    }
}

// ── Copy assignment ───────────────────────────────────────────────────────────
Row& Row::operator=(const Row& other) {
    if (this == &other) return *this;  // self-assignment guard
    freeFields();
    numCols = other.numCols;
    cols    = new Field*[numCols];
    for (int i = 0; i < numCols; i++) {
        cols[i] = other.cols[i]->clone();
    }
    return *this;
}

// ── setField ──────────────────────────────────────────────────────────────────
void Row::setField(int idx, const Field* field) {
    if (idx < 0 || idx >= numCols) return;
    delete cols[idx];
    cols[idx] = field->clone();
}

// ── getField ──────────────────────────────────────────────────────────────────
const Field* Row::getField(int idx) const {
    if (idx < 0 || idx >= numCols) return nullptr;
    return cols[idx];
}

// ── serialize ─────────────────────────────────────────────────────────────────
void Row::serialize(char* buf, const Schema* schema) const {
    int offset = 0;
    for (int i = 0; i < numCols; i++) {
        // Write field bytes at the current offset; advance by field width.
        cols[i]->serialize(buf + offset);
        offset += schema->widthOf(i);
    }
}

// ── deserialize ───────────────────────────────────────────────────────────────
void Row::deserialize(const char* buf, const Schema* schema) {
    int offset = 0;
    for (int i = 0; i < numCols; i++) {
        // Read field bytes from the current offset; advance by field width.
        cols[i]->deserialize(buf + offset);
        offset += schema->widthOf(i);
    }
}

// ── print ─────────────────────────────────────────────────────────────────────
void Row::print() const {
    printf("| ");
    for (int i = 0; i < numCols; i++) {
        cols[i]->print();
        printf(" | ");
    }
    printf("\n");
}
