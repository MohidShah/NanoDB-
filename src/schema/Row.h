/*
 * Row.h
 * -----
 * A Row is one record in a table — an ordered collection of Field* values
 * matching the column definitions in a Schema.
 *
 * ── Memory layout ────────────────────────────────────────────────────────────
 *   Field** cols  →  heap array of Field* pointers
 *   cols[0]       →  heap-allocated IntField / FloatField / StringField
 *   cols[1]       →  ...
 *
 * Ownership: the Row owns each Field* in cols[].
 *   Constructor: allocates Field objects based on Schema column types.
 *   Destructor:  deletes each Field*, then deletes[] cols.
 *
 * ── Serialization ────────────────────────────────────────────────────────────
 * serialize(buf):   writes each field's bytes consecutively into buf.
 * deserialize(buf): reads bytes back and reconstructs each Field.
 *
 * Total bytes = Schema::rowWidth() (sum of all field widths).
 */
#ifndef ROW_H
#define ROW_H

#include "Field.h"
#include "Schema.h"

class Row {
public:
    Field**     cols;     // polymorphic field array (owned by this Row)
    int         numCols;

    /**
     * Construct an empty row matching the given schema.
     * Allocates one Field* per column (default zero/empty values).
     */
    explicit Row(const Schema* schema);

    /**
     * Destructor — deletes every Field* and then the cols[] array.
     * This is the critical memory-safety function: no leaks allowed.
     */
    ~Row();

    /**
     * Deep-copy constructor — clones each Field so two Rows never
     * share heap-allocated Field objects (avoids double-free).
     */
    Row(const Row& other);

    /** Deep-copy assignment. */
    Row& operator=(const Row& other);

    // ── Data access ───────────────────────────────────────────────────────────

    /** Set column `idx` to the value of `field` (clones field). */
    void setField(int idx, const Field* field);

    /** Get a const pointer to column `idx`. */
    const Field* getField(int idx) const;

    // ── Serialization ─────────────────────────────────────────────────────────

    /**
     * serialize() — write this row's bytes into `buf`.
     * buf must be at least Schema::rowWidth() bytes long.
     */
    void serialize(char* buf, const Schema* schema) const;

    /**
     * deserialize() — reconstruct field values from `buf`.
     * buf must contain exactly Schema::rowWidth() bytes of valid data.
     */
    void deserialize(const char* buf, const Schema* schema);

    /** Print all field values separated by '|'. */
    void print() const;

private:
    void freeFields();
    void allocFields(const Schema* schema);
};

#endif // ROW_H
