/*
 * Schema.h
 * --------
 * Defines the structure of a table: how many columns, their names, and types.
 *
 * A Schema is created once per table and shared across all Row objects
 * belonging to that table.  It is also serialized to the SystemCatalog
 * so the engine can reconstruct a table's structure after reboot (Test G).
 *
 * Design: a plain C-style struct array (ColumnDef[]) — no STL containers.
 *
 * Viva Q: "How does the engine know the byte offset of column 3 in a row?"
 *   Schema::offsetOf(colIndex) walks columns 0..colIndex-1, summing their
 *   serializedWidth().  This is O(numCols) but numCols is tiny (≤ 20).
 */
#ifndef SCHEMA_H
#define SCHEMA_H

#include "Field.h"
#include <cstring>
#include <cstdio>

static const int MAX_COLS       = 32;   // max columns per table
static const int MAX_TABLE_NAME = 64;
static const int MAX_COL_NAME   = 64;

struct ColumnDef {
    char       name[MAX_COL_NAME];
    Field::Type type;
};

class Schema {
public:
    char      tableName[MAX_TABLE_NAME];
    ColumnDef columns[MAX_COLS];
    int       numCols;

    Schema() : numCols(0) {
        std::memset(tableName, 0, sizeof(tableName));
        std::memset(columns,   0, sizeof(columns));
    }

    /** Add a column definition. Returns false if MAX_COLS exceeded. */
    bool addColumn(const char* name, Field::Type type) {
        if (numCols >= MAX_COLS) return false;
        std::strncpy(columns[numCols].name, name, MAX_COL_NAME - 1);
        columns[numCols].type = type;
        numCols++;
        return true;
    }

    /** Find a column index by name. Returns -1 if not found. */
    int indexOf(const char* name) const {
        for (int i = 0; i < numCols; i++) {
            if (std::strcmp(columns[i].name, name) == 0) return i;
        }
        return -1;
    }

    /**
     * Byte offset of column `col` within a serialized row.
     * Computed by summing widths of all preceding columns.
     */
    int offsetOf(int col) const {
        int offset = 0;
        for (int i = 0; i < col && i < numCols; i++) {
            offset += widthOf(i);
        }
        return offset;
    }

    /** Width in bytes of a column based on its type. */
    int widthOf(int col) const {
        if (col < 0 || col >= numCols) return 0;
        switch (columns[col].type) {
            case Field::INT:    return INT_FIELD_WIDTH;
            case Field::FLOAT:  return FLOAT_FIELD_WIDTH;
            case Field::STRING: return STRING_FIELD_WIDTH;
        }
        return 0;
    }

    /** Total bytes needed to store one row from this schema. */
    int rowWidth() const {
        int total = 0;
        for (int i = 0; i < numCols; i++) total += widthOf(i);
        return total;
    }

    /** Max rows that fit on a single 4KB page (after the 8-byte header). */
    int rowsPerPage() const {
        int rw = rowWidth();
        if (rw == 0) return 0;
        return (PAGE_SIZE - PAGE_HEADER_SZ) / rw;
    }

    void print() const {
        printf("Schema [%s]: ", tableName);
        for (int i = 0; i < numCols; i++) {
            const char* typeName =
                (columns[i].type == Field::INT)    ? "INT"    :
                (columns[i].type == Field::FLOAT)  ? "FLOAT"  : "STRING";
            printf("%s(%s)", columns[i].name, typeName);
            if (i < numCols - 1) printf(", ");
        }
        printf("\n");
    }
};

#endif // SCHEMA_H
