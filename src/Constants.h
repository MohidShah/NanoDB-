/*
 * Constants.h
 * -----------
 * Central place for compile-time constants shared across ALL layers of NanoDB.
 *
 * Including this one header in any file gives access to page size constants,
 * max limits, and other engine-wide values — without creating circular
 * include dependencies between the memory/ and schema/ layers.
 *
 * Rule: add a constant here only if it is referenced by more than one layer.
 */
#ifndef CONSTANTS_H
#define CONSTANTS_H

// ── Page / Buffer Pool ────────────────────────────────────────────────────────
static const int PAGE_SIZE      = 4096;   // bytes per page (matches OS virtual page)
static const int PAGE_HEADER_SZ = 8;      // bytes reserved for page header (id + rowCount)

// ── Table limits ──────────────────────────────────────────────────────────────
static const int MAX_TABLE_PAGES = 4096;  // max pages per table
static const int MAX_COLS        = 32;    // max columns per table
static const int MAX_TABLE_NAME  = 64;    // max length of a table name
static const int MAX_COL_NAME    = 64;    // max length of a column name

// ── Field widths (fixed-width binary serialization) ───────────────────────────
static const int INT_FIELD_WIDTH    = 4;   // sizeof(int32_t)
static const int FLOAT_FIELD_WIDTH  = 8;   // sizeof(double)
static const int STRING_FIELD_WIDTH = 256; // fixed-width null-padded char array

// ── Hash map ──────────────────────────────────────────────────────────────────
static const int HT_CAPACITY = 1024;   // LRU hash table bucket count

// ── Priority levels ───────────────────────────────────────────────────────────
static const int PRIORITY_ADMIN = 10;
static const int PRIORITY_USER  = 1;

#endif // CONSTANTS_H
