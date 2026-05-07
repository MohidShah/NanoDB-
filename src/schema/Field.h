/*
 * Field.h
 * -------
 * Abstract base class for all column data types in NanoDB.
 *
 * ── The Problem ───────────────────────────────────────────────────────────────
 * A database row contains mixed types: an int ID, a double balance, a string
 * name.  A plain C array requires uniform types.  To store heterogeneous data
 * in a single Row object we use POLYMORPHISM:
 *
 *   Field* cols[3];
 *   cols[0] = new IntField(42);
 *   cols[1] = new FloatField(9999.99);
 *   cols[2] = new StringField("Alice");
 *
 * Each element is a Field*, but at runtime the vtable dispatches calls to the
 * correct derived-class method (IntField::compare, StringField::serialize, …).
 *
 * ── Operator Overloading ──────────────────────────────────────────────────────
 * The query evaluator needs to compare field values without knowing their
 * concrete type:
 *
 *   if (*row->cols[0] > *threshold) { ... }
 *
 * Each derived class overloads >, <, ==, !=, >=, <= so the evaluator never
 * needs hardcoded if-statements per type.
 *
 * ── Serialization ────────────────────────────────────────────────────────────
 * Fixed-width binary layout (enables O(1) field offset calculation):
 *   IntField    → 4 bytes  (int32)
 *   FloatField  → 8 bytes  (double)
 *   StringField → 256 bytes (null-padded char array)
 *
 */
#ifndef FIELD_H
#define FIELD_H

#include <string.h>  // C header (avoids Clangd false positives on MinGW)   // strcmp, strncpy, strlen
#include <stdio.h>   // C header    // printf, sprintf
#include "../Constants.h"  // INT_FIELD_WIDTH, FLOAT_FIELD_WIDTH, STRING_FIELD_WIDTH

// ── Abstract base ──────────────────────────────────────────────────────────────
class Field {
public:
    enum Type { INT, FLOAT, STRING };

    virtual Type getType() const = 0;

    // ── Comparison operators (overloaded in each derived class) ───────────────
    virtual bool operator>(const Field& o)  const = 0;
    virtual bool operator<(const Field& o)  const = 0;
    virtual bool operator==(const Field& o) const = 0;
    virtual bool operator!=(const Field& o) const { return !(*this == o); }
    virtual bool operator>=(const Field& o) const { return !(*this < o);  }
    virtual bool operator<=(const Field& o) const { return !(*this > o);  }

    // ── Serialization ─────────────────────────────────────────────────────────
    /** Write this field's bytes into buf (caller must ensure enough space). */
    virtual void serialize(char* buf)         const = 0;
    /** Reconstruct this field's value from buf. */
    virtual void deserialize(const char* buf)       = 0;
    /** How many bytes this field occupies in a serialized row. */
    virtual int  serializedWidth()            const = 0;

    // ── Utilities ─────────────────────────────────────────────────────────────
    /** Deep copy — needed when duplicating rows (e.g., join results). */
    virtual Field* clone() const = 0;
    /** Human-readable print for console output. */
    virtual void   print()  const = 0;
    /** Returns numeric value for arithmetic expressions (e.g., o_totalprice * 1.5). */
    virtual double toDouble() const = 0;
    /** Returns string value (for StringField comparisons in expressions). */
    virtual void   toString(char* out, int maxLen) const = 0;

    virtual ~Field() {}
};

// ═════════════════════════════════════════════════════════════════════════════
// IntField
// ═════════════════════════════════════════════════════════════════════════════
class IntField : public Field {
public:
    int val;

    IntField()           : val(0)   {}
    explicit IntField(int v) : val(v) {}

    Type getType() const override { return INT; }

    // Operators compare int values directly.
    bool operator>(const Field& o)  const override { return val >  static_cast<const IntField&>(o).val; }
    bool operator<(const Field& o)  const override { return val <  static_cast<const IntField&>(o).val; }
    bool operator==(const Field& o) const override { return val == static_cast<const IntField&>(o).val; }

    void serialize(char* buf)         const override { memcpy(buf, &val, sizeof(int)); }
    void deserialize(const char* buf)       override { memcpy(&val, buf, sizeof(int)); }
    int  serializedWidth()            const override { return INT_FIELD_WIDTH; }

    Field* clone()  const override { return new IntField(val); }
    void   print()  const override { printf("%d", val); }
    double toDouble() const override { return static_cast<double>(val); }
    void   toString(char* out, int maxLen) const override { snprintf(out, maxLen, "%d", val); }
};

// ═════════════════════════════════════════════════════════════════════════════
// FloatField
// ═════════════════════════════════════════════════════════════════════════════
class FloatField : public Field {
public:
    double val;

    FloatField()              : val(0.0) {}
    explicit FloatField(double v) : val(v)  {}

    Type getType() const override { return FLOAT; }

    bool operator>(const Field& o)  const override { return val >  static_cast<const FloatField&>(o).val; }
    bool operator<(const Field& o)  const override { return val <  static_cast<const FloatField&>(o).val; }
    bool operator==(const Field& o) const override { return val == static_cast<const FloatField&>(o).val; }

    void serialize(char* buf)         const override { memcpy(buf, &val, sizeof(double)); }
    void deserialize(const char* buf)       override { memcpy(&val, buf, sizeof(double)); }
    int  serializedWidth()            const override { return FLOAT_FIELD_WIDTH; }

    Field* clone()  const override { return new FloatField(val); }
    void   print()  const override { printf("%.4f", val); }
    double toDouble() const override { return val; }
    void   toString(char* out, int maxLen) const override { snprintf(out, maxLen, "%.4f", val); }
};

// ═════════════════════════════════════════════════════════════════════════════
// StringField
// ═════════════════════════════════════════════════════════════════════════════
class StringField : public Field {
public:
    char val[STRING_FIELD_WIDTH];  // fixed-width null-padded buffer

    StringField()  { memset(val, 0, STRING_FIELD_WIDTH); }
    explicit StringField(const char* s) {
        memset(val, 0, STRING_FIELD_WIDTH);
        strncpy(val, s, STRING_FIELD_WIDTH - 1);
    }

    Type getType() const override { return STRING; }

    // strcmp returns 0 for equal, <0 for less-than, >0 for greater-than.
    bool operator>(const Field& o)  const override { return strcmp(val, static_cast<const StringField&>(o).val) >  0; }
    bool operator<(const Field& o)  const override { return strcmp(val, static_cast<const StringField&>(o).val) <  0; }
    bool operator==(const Field& o) const override { return strcmp(val, static_cast<const StringField&>(o).val) == 0; }

    void serialize(char* buf)         const override { memcpy(buf, val, STRING_FIELD_WIDTH); }
    void deserialize(const char* buf)       override { memcpy(val, buf, STRING_FIELD_WIDTH); }
    int  serializedWidth()            const override { return STRING_FIELD_WIDTH; }

    Field* clone()  const override { return new StringField(val); }
    void   print()  const override { printf("%s", val); }
    double toDouble() const override { return 0.0; }  // strings don't convert to numeric
    void   toString(char* out, int maxLen) const override { strncpy(out, val, maxLen - 1); out[maxLen-1] = '\0'; }
};

#endif // FIELD_H
