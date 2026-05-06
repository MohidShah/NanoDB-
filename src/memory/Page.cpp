/*
 * Page.cpp
 * --------
 * Implementation of the Page class.
 *
 * Pointer arithmetic cheat-sheet (for viva):
 *   `data`          → points to byte 0 of the 4-KB block
 *   `data + offset` → points to byte `offset` (valid for 0 <= offset < PAGE_SIZE)
 *   memcpy(dst, src, n) copies n bytes starting at src into dst
 *   memset(ptr, val, n) fills n bytes starting at ptr with val
 */
#include "Page.h"
#include <cstring>  // memset, memcpy

// ── Constructor ───────────────────────────────────────────────────────────────
Page::Page(int id) : pageId(id), isDirty(false), pinCount(0) {
    // Allocate the raw 4-KB block.
    // new char[PAGE_SIZE] returns a char* pointing to the first byte.
    data = new char[PAGE_SIZE];
    clear();         // zero-fill: avoids reading garbage from uninitialised memory
    writeHeader();   // embed pageId into bytes 0-3 for self-identification on disk
}

// ── Destructor ────────────────────────────────────────────────────────────────
Page::~Page() {
    // delete[] is REQUIRED here (not delete) because data was allocated
    // with new char[PAGE_SIZE] (array form).  Using delete without []
    // is undefined behaviour and a guaranteed memory bug.
    delete[] data;
    data = nullptr;  // poison the pointer so use-after-free is detectable
}

// ── Data access ───────────────────────────────────────────────────────────────

bool Page::write(const char* src, int offset, int size) {
    // Guard: reject out-of-bounds writes
    if (offset < 0 || size <= 0 || (offset + size) > PAGE_SIZE) {
        return false;
    }
    // data + offset → raw address of destination byte within this page
    std::memcpy(data + offset, src, static_cast<size_t>(size));
    isDirty = true;  // mark modified — Buffer Pool will flush on eviction
    return true;
}

bool Page::read(char* dst, int offset, int size) const {
    if (offset < 0 || size <= 0 || (offset + size) > PAGE_SIZE) {
        return false;
    }
    std::memcpy(dst, data + offset, static_cast<size_t>(size));
    return true;
}

void Page::clear() {
    // Fill every byte with 0x00.
    // sizeof(char) == 1, so PAGE_SIZE bytes is exactly the block size.
    std::memset(data, 0, PAGE_SIZE);
    isDirty = false;
}

// ── Header helpers ────────────────────────────────────────────────────────────

void Page::writeHeader() {
    // Store pageId as 4 raw bytes at offset 0.
    // reinterpret_cast is necessary to treat the int's bytes as chars.
    std::memcpy(data + 0, &pageId, sizeof(int));
}

int Page::readHeaderId() const {
    int id = 0;
    std::memcpy(&id, data + 0, sizeof(int));
    return id;
}

void Page::setRowCount(int count) {
    // Store row count as 4 raw bytes at offset 4 (after the page ID).
    std::memcpy(data + sizeof(int), &count, sizeof(int));
    isDirty = true;
}

int Page::getRowCount() const {
    int count = 0;
    std::memcpy(&count, data + sizeof(int), sizeof(int));
    return count;
}
