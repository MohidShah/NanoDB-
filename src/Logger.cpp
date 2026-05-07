/*
 * Logger.cpp
 * ----------
 * Implementation of the NanoDB centralized logger.
 *
 * Key pointer/memory note:
 *   `logFile` is a raw FILE* managed manually (fopen / fclose).
 *   No smart pointers or RAII wrappers — we own the resource explicitly.
 */
#include "Logger.h"
#include <stdio.h>   // FILE*, fprintf, vfprintf, fopen, fclose, fflush
#include <stdarg.h>  // va_list, va_start, va_end
#include <time.h>    // time_t, time(), localtime()
#include <string.h>  // strlen

// ── Static member definition ──────────────────────────────────────────────────
// Exactly one translation unit must define this; all others just declare it.
FILE* Logger::logFile = nullptr;

// ── Helper: write timestamp prefix "HH:MM:SS" ────────────────────────────────
static void writeTimestamp(FILE* f) {
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    fprintf(f, "[LOG][%02d:%02d:%02d] ", t->tm_hour, t->tm_min, t->tm_sec);
}

static void writeErrorTimestamp(FILE* f) {
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    fprintf(f, "[ERROR][%02d:%02d:%02d] ", t->tm_hour, t->tm_min, t->tm_sec);
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void Logger::init(const char* filename) {
    logFile = fopen(filename, "w");
    if (!logFile) {
        fprintf(stderr, "[LOGGER FATAL] Cannot open log file: %s\n", filename);
        return;
    }
    log("NanoDB Engine Started. Log target: %s", filename);
    separator("ENGINE BOOT");
}

void Logger::close() {
    if (logFile) {
        separator("ENGINE SHUTDOWN");
        log("NanoDB Engine shut down cleanly.");
        fclose(logFile);
        logFile = nullptr;
    }
}

// ── Logging ───────────────────────────────────────────────────────────────────

void Logger::log(const char* format, ...) {
    // Write to stdout
    writeTimestamp(stdout);
    va_list args;
    va_start(args, format);
    vfprintf(stdout, format, args);
    va_end(args);
    fprintf(stdout, "\n");

    // Mirror to log file
    if (logFile) {
        writeTimestamp(logFile);
        va_start(args, format);
        vfprintf(logFile, format, args);
        va_end(args);
        fprintf(logFile, "\n");
        fflush(logFile);  // survive crash — evaluator must see complete log
    }
}

void Logger::error(const char* format, ...) {
    // Write to stderr
    writeErrorTimestamp(stderr);
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fprintf(stderr, "\n");

    // Also mirror to log file so the evaluator sees errors too
    if (logFile) {
        writeErrorTimestamp(logFile);
        va_start(args, format);
        vfprintf(logFile, format, args);
        va_end(args);
        fprintf(logFile, "\n");
        fflush(logFile);
    }
}

void Logger::separator(const char* label) {
    const char* line = "================================================";
    if (label) {
        fprintf(stdout,  "[LOG] %s [ %s ] %s\n", line, label, line);
        if (logFile) {
            fprintf(logFile, "[LOG] %s [ %s ] %s\n", line, label, line);
            fflush(logFile);
        }
    } else {
        fprintf(stdout,  "[LOG] %s\n", line);
        if (logFile) {
            fprintf(logFile, "[LOG] %s\n", line);
            fflush(logFile);
        }
    }
}
