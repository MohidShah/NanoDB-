/*
 * QueryExecutor.cpp
 */
#include "QueryExecutor.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>   // atoi, atof

// ─────────────────────────────────────────────────────────────────────────────
// Local helper: case-insensitive string equality
// ─────────────────────────────────────────────────────────────────────────────
static bool strEqCI(const char* a, const char* b) {
    while (*a && *b) {
        char ca = (*a >= 'A' && *a <= 'Z') ? (char)(*a + 32) : *a;
        char cb = (*b >= 'A' && *b <= 'Z') ? (char)(*b + 32) : *b;
        if (ca != cb) return false;
        a++; b++;
    }
    return *a == '\0' && *b == '\0';
}

// ─────────────────────────────────────────────────────────────────────────────
// Scan callbacks (static — used as RowFilter function pointers)
// ─────────────────────────────────────────────────────────────────────────────
bool QueryExecutor::whereFilter(const Row* row, void* ctx) {
    WhereCtx* w = static_cast<WhereCtx*>(ctx);
    if (w->tokenCount == 0) return true;
    bool err = false;
    bool result = w->evaluator->evaluate(w->postfix, w->tokenCount, row, w->schema, &err);
    return !err && result;
}

bool QueryExecutor::printMatch(const Row* row, void* ctx) {
    WhereCtx* w = static_cast<WhereCtx*>(ctx);
    if (w->printRows) row->print();
    w->matchCount++;
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Constructor / Destructor
// ─────────────────────────────────────────────────────────────────────────────
QueryExecutor::QueryExecutor(BufferPool* p, SystemCatalog* c)
    : pool(p), catalog(c), indexCount(0) {}

QueryExecutor::~QueryExecutor() {
    for (int i = 0; i < indexCount; i++) {
        delete indexes[i].tree;
        indexes[i].tree = 0;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// submit / runAll / execute
// ─────────────────────────────────────────────────────────────────────────────
void QueryExecutor::submit(const char* sql) {
    Tokenizer tok;
    tok.tokenize(sql);
    // UPDATE → admin priority (Test Case E)
    bool isUpdate = (tok.count > 0 &&
                     tok.tokens[0].type == TOK_IDENT &&
                     strEqCI(tok.tokens[0].lexeme, "UPDATE"));
    int prio = isUpdate ? PRIORITY_ADMIN : PRIORITY_USER;
    if (prio == PRIORITY_ADMIN)
        Logger::log("Admin UPDATE intercepted, promoted to front of queue.");
    queue.insert(prio, sql);
}

void QueryExecutor::runAll() {
    while (!queue.isEmpty()) {
        QueryTask task = queue.extractMax();
        Logger::log("Executing [prio=%d]: %s", task.priority, task.sql);
        execute(task.sql);
    }
}

void QueryExecutor::execute(const char* sql) {
    Tokenizer tok;
    tok.tokenize(sql);
    if (tok.count == 0) return;

    const Token* t = tok.tokens;
    int n = tok.count;

    if (t[0].type == TOK_SELECT) {
        // Multi-table join: look for comma after table name (after FROM)
        bool isJoin = false;
        for (int i = 1; i < n; i++) {
            if (t[i].type == TOK_FROM && i+2 < n && t[i+2].type == TOK_COMMA) {
                isJoin = true; break;
            }
        }
        if (isJoin) execJoin(t, n);
        else         execSelect(t, n);
    }
    else if (t[0].type == TOK_INSERT)                              execInsert(t, n);
    else if (t[0].type == TOK_CREATE)                              execCreate(t, n);
    else if (t[0].type == TOK_DROP)                                execDrop  (t, n);
    else if (t[0].type == TOK_LIST)                                execList  (t, n);
    else if (t[0].type == TOK_IDENT && strEqCI(t[0].lexeme,"UPDATE")) execUpdate(t, n);
    else if (t[0].type == TOK_IDENT && strEqCI(t[0].lexeme,"DELETE")) execDelete(t, n);
    else Logger::error("Unknown command: %s", t[0].lexeme);
}

// ─────────────────────────────────────────────────────────────────────────────
// CREATE TABLE name (col TYPE, ...)
// ─────────────────────────────────────────────────────────────────────────────
void QueryExecutor::execCreate(const Token* t, int n) {
    if (n < 5 || t[1].type != TOK_TABLE) {
        Logger::error("CREATE TABLE syntax error."); return;
    }
    const char* tname = t[2].lexeme;
    if (catalog->hasTable(tname)) {
        Logger::error("Table '%s' already exists.", tname); return;
    }
    Schema* schema = new Schema();
    strncpy(schema->tableName, tname, MAX_TABLE_NAME - 1);

    int i = 4; // skip past '('
    while (i < n && t[i].type != TOK_RPAREN && t[i].type != TOK_EOF) {
        if (t[i].type == TOK_COMMA) { i++; continue; }
        const char* colName = t[i].lexeme; i++;
        if (i >= n) break;
        Field::Type ftype = Field::INT;
        if      (t[i].type == TOK_FLOAT_KW)  ftype = Field::FLOAT;
        else if (t[i].type == TOK_STRING_KW) ftype = Field::STRING;
        schema->addColumn(colName, ftype);
        i++;
    }

    char dataFile[300];
    snprintf(dataFile, sizeof(dataFile), "data/%s.dbf", tname);

    Table* table = catalog->createTable(schema, dataFile);
    if (!table) {
        Logger::error("Failed to create table '%s'.", tname);
        return;
    }
    Logger::log("CREATE TABLE %s (%d columns).", tname, schema->numCols);
}

// ─────────────────────────────────────────────────────────────────────────────
// DROP TABLE
// ─────────────────────────────────────────────────────────────────────────────
void QueryExecutor::execDrop(const Token* t, int n) {
    if (n < 3 || t[1].type != TOK_TABLE) {
        Logger::error("DROP TABLE syntax error."); return;
    }
    const char* tname = t[2].lexeme;
    if (!catalog->dropTable(tname))
        Logger::error("DROP TABLE: '%s' not found.", tname);
    else
        Logger::log("DROP TABLE %s.", tname);
}

// ─────────────────────────────────────────────────────────────────────────────
// LIST TABLES
// ─────────────────────────────────────────────────────────────────────────────
void QueryExecutor::execList(const Token* /*t*/, int /*n*/) {
    Logger::log("LIST TABLES (%d registered):", catalog->tableCount());
    catalog->listTables();
}

// ─────────────────────────────────────────────────────────────────────────────
// INSERT INTO tname VALUES (v1, v2, ...)
// ─────────────────────────────────────────────────────────────────────────────
void QueryExecutor::execInsert(const Token* t, int n) {
    if (n < 5 || t[1].type != TOK_INTO || t[3].type != TOK_VALUES) {
        Logger::error("INSERT syntax error."); return;
    }
    const char* tname = t[2].lexeme;
    Table* table = catalog->getTable(tname);
    if (!table) { Logger::error("INSERT: table '%s' not found.", tname); return; }

    Row* row = new Row(table->schema);
    int col = 0;
    int i = 5; // skip past '('
    while (i < n && t[i].type != TOK_RPAREN && t[i].type != TOK_EOF) {
        if (t[i].type == TOK_COMMA) { i++; continue; }
        if (col >= table->schema->numCols) break;
        Field::Type ftype = table->schema->columns[col].type;
        if (ftype == Field::INT) {
            IntField f(t[i].ival);
            row->setField(col, &f);
        } else if (ftype == Field::FLOAT) {
            double dv = (t[i].type == TOK_FLOAT_LIT) ? t[i].dval : (double)t[i].ival;
            FloatField f(dv);
            row->setField(col, &f);
        } else {
            StringField f(t[i].lexeme);
            row->setField(col, &f);
        }
        col++; i++;
    }

    int pageId = -1, slotId = -1;
    bool ok = table->insertRow(row, &pageId, &slotId);
    if (ok) {
        Logger::log("INSERT INTO %s: row at page=%d slot=%d.", tname, pageId, slotId);
        // Update AVL index if present
        for (int idx = 0; idx < indexCount; idx++) {
            if (strcmp(indexes[idx].tableName, tname) == 0) {
                const Field* f = row->getField(indexes[idx].colIndex);
                if (f) indexes[idx].tree->insert(f->toDouble(), pageId, slotId);
                break;
            }
        }
    } else {
        Logger::error("INSERT INTO %s: failed.", tname);
    }
    delete row;
}

// ─────────────────────────────────────────────────────────────────────────────
// AVL index helpers
// ─────────────────────────────────────────────────────────────────────────────
void QueryExecutor::buildIndex(Table* table, AVLTree* tree, int colIdx) {
    for (int p = 0; p < table->numPages; p++) {
        int pid = table->pageDirectory[p];
        Page* page = pool->fetchPage(pid);
        if (!page) continue;
        int rowCount = page->getRowCount();
        pool->releasePage(pid);
        for (int s = 0; s < rowCount; s++) {
            Row* row = table->getRow(pid, s);
            if (!row) continue;
            const Field* f = row->getField(colIdx);
            if (f) tree->insert(f->toDouble(), pid, s);
            delete row;
        }
    }
    Logger::log("AVL index built on '%s' col[%d], %d node(s).",
                table->schema->tableName, colIdx, tree->size());
}

AVLTree* QueryExecutor::getOrBuildIndex(Table* table, const char* tableName) {
    for (int i = 0; i < indexCount; i++)
        if (strcmp(indexes[i].tableName, tableName) == 0)
            return indexes[i].tree;

    // Find first INT column
    int ci = -1;
    for (int i = 0; i < table->schema->numCols; i++) {
        if (table->schema->columns[i].type == Field::INT) { ci = i; break; }
    }
    if (ci < 0 || indexCount >= MAX_INDEXED_TABLES) return 0;

    AVLTree* tree = new AVLTree();
    buildIndex(table, tree, ci);
    strncpy(indexes[indexCount].tableName, tableName, MAX_TABLE_NAME - 1);
    indexes[indexCount].colIndex = ci;
    indexes[indexCount].tree     = tree;
    indexCount++;
    return tree;
}

// ─────────────────────────────────────────────────────────────────────────────
// SELECT * FROM tname [WHERE expr]
// ─────────────────────────────────────────────────────────────────────────────
void QueryExecutor::execSelect(const Token* t, int n) {
    int fromIdx = -1;
    for (int i = 1; i < n; i++) if (t[i].type == TOK_FROM) { fromIdx = i; break; }
    if (fromIdx < 0) { Logger::error("SELECT: missing FROM."); return; }

    const char* tname = t[fromIdx+1].lexeme;
    Table* table = catalog->getTable(tname);
    if (!table) { Logger::error("SELECT: table '%s' not found.", tname); return; }

    int whereIdx = -1;
    for (int i = fromIdx+2; i < n; i++)
        if (t[i].type == TOK_WHERE) { whereIdx = i; break; }

    ShuntingYard sy;
    if (whereIdx >= 0) {
        sy.convert(t + whereIdx + 1, n - whereIdx - 1);

        // Test Case A — log infix → postfix conversion
        char infixBuf[512] = {0}, postBuf[512] = {0};
        int off = 0;
        for (int i = whereIdx+1; i < n && t[i].type != TOK_EOF && t[i].type != TOK_SEMICOLON; i++)
            off += snprintf(infixBuf+off, (int)sizeof(infixBuf)-off, "%s ", t[i].lexeme);
        off = 0;
        for (int i = 0; i < sy.outCount; i++)
            off += snprintf(postBuf+off, (int)sizeof(postBuf)-off, "%s ", sy.output[i].lexeme);
        Logger::log("Infix \"%s\" converted to Postfix \"%s\"", infixBuf, postBuf);
    }

    ExprEvaluator eval;
    WhereCtx wctx;
    wctx.postfix    = sy.output;
    wctx.tokenCount = sy.outCount;
    wctx.schema     = table->schema;
    wctx.evaluator  = &eval;
    wctx.matchCount = 0;
    wctx.printRows  = true;

    // Test Case B — try AVL index for simple equality: col = int_literal
    bool usedIndex = false;
    if (whereIdx >= 0 && sy.outCount == 3 &&
        sy.output[0].type == TOK_IDENT &&
        sy.output[1].type == TOK_INT_LIT &&
        sy.output[2].type == TOK_EQ)
    {
        AVLTree* tree = getOrBuildIndex(table, tname);
        if (tree) {
            int indexedCol = -1;
            for (int i = 0; i < indexCount; i++)
                if (strcmp(indexes[i].tableName, tname)==0) { indexedCol=indexes[i].colIndex; break; }
            int queryCol = table->schema->indexOf(sy.output[0].lexeme);
            if (queryCol == indexedCol) {
                double key = (double)sy.output[1].ival;
                RID rid = tree->search(key);
                Logger::log("AVL index lookup key=%.0f → page=%d slot=%d",
                            key, rid.pageId, rid.slotId);
                if (rid.valid()) {
                    Row* row = table->getRow(rid.pageId, rid.slotId);
                    if (row) { row->print(); wctx.matchCount++; delete row; }
                }
                usedIndex = true;
            }
        }
    }

    if (!usedIndex)
        table->scan(whereFilter, &wctx, printMatch, &wctx);

    Logger::log("SELECT FROM %s: %d row(s) returned.", tname, wctx.matchCount);
}

// ─────────────────────────────────────────────────────────────────────────────
// UPDATE tname SET col=val [WHERE expr]
// ─────────────────────────────────────────────────────────────────────────────
void QueryExecutor::execUpdate(const Token* t, int n) {
    // t[0]=UPDATE  t[1]=tname  t[2]=SET  t[3]=colName  t[4]==  t[5]=val
    if (n < 6) { Logger::error("UPDATE syntax error."); return; }
    const char* tname = t[1].lexeme;
    Table* table = catalog->getTable(tname);
    if (!table) { Logger::error("UPDATE: table '%s' not found.", tname); return; }

    // locate SET keyword (token type is TOK_IDENT "SET")
    int setIdx = -1;
    for (int i = 2; i < n; i++) {
        if (t[i].type == TOK_IDENT && strEqCI(t[i].lexeme, "SET")) { setIdx = i; break; }
    }
    if (setIdx < 0 || setIdx + 3 >= n) { Logger::error("UPDATE: missing SET."); return; }

    const char* setColName = t[setIdx+1].lexeme;
    int setColIdx = table->schema->indexOf(setColName);
    if (setColIdx < 0) { Logger::error("UPDATE: col '%s' not found.", setColName); return; }
    // t[setIdx+2] = '=' (TOK_EQ), t[setIdx+3] = new value
    const char* newVal = t[setIdx+3].lexeme;

    int whereIdx = -1;
    for (int i = setIdx+4; i < n; i++) if (t[i].type == TOK_WHERE) { whereIdx = i; break; }

    ShuntingYard sy;
    if (whereIdx >= 0) sy.convert(t + whereIdx + 1, n - whereIdx - 1);

    ExprEvaluator eval;
    int updated = 0;

    for (int p = 0; p < table->numPages; p++) {
        int pid = table->pageDirectory[p];
        Page* page = pool->fetchPage(pid);
        if (!page) continue;
        int rowCount = page->getRowCount();
        pool->releasePage(pid);

        for (int s = 0; s < rowCount; s++) {
            Row* row = table->getRow(pid, s);
            if (!row) continue;
            bool match = true;
            if (whereIdx >= 0) {
                bool err = false;
                match = eval.evaluate(sy.output, sy.outCount, row, table->schema, &err);
                if (err) match = false;
            }
            if (match) {
                Field::Type ftype = table->schema->columns[setColIdx].type;
                if (ftype == Field::INT) {
                    IntField f(atoi(newVal)); row->setField(setColIdx, &f);
                } else if (ftype == Field::FLOAT) {
                    FloatField f(atof(newVal)); row->setField(setColIdx, &f);
                } else {
                    StringField f(newVal); row->setField(setColIdx, &f);
                }
                table->updateRow(pid, s, row);
                updated++;
            }
            delete row;
        }
    }
    Logger::log("UPDATE %s SET %s=%s: %d row(s) modified.", tname, setColName, newVal, updated);
}

// ─────────────────────────────────────────────────────────────────────────────
// DELETE FROM tname [WHERE expr]
// ─────────────────────────────────────────────────────────────────────────────
void QueryExecutor::execDelete(const Token* t, int n) {
    // t[0]=DELETE  t[1]=FROM  t[2]=tname
    if (n < 3) { Logger::error("DELETE syntax error."); return; }
    const char* tname = t[2].lexeme;
    Table* table = catalog->getTable(tname);
    if (!table) { Logger::error("DELETE: table '%s' not found.", tname); return; }

    int whereIdx = -1;
    for (int i = 3; i < n; i++) if (t[i].type == TOK_WHERE) { whereIdx = i; break; }

    ShuntingYard sy;
    if (whereIdx >= 0) sy.convert(t + whereIdx + 1, n - whereIdx - 1);

    ExprEvaluator eval;
    int deleted = 0;

    for (int p = 0; p < table->numPages; p++) {
        int pid = table->pageDirectory[p];
        Page* page = pool->fetchPage(pid);
        if (!page) continue;
        int rowCount = page->getRowCount();
        pool->releasePage(pid);

        for (int s = 0; s < rowCount; s++) {
            Row* row = table->getRow(pid, s);
            if (!row) continue;
            bool match = true;
            if (whereIdx >= 0) {
                bool err = false;
                match = eval.evaluate(sy.output, sy.outCount, row, table->schema, &err);
                if (err) match = false;
            }
            if (match) {
                // Logical delete: zero out row bytes and mark dirty
                Page* pg = pool->fetchPage(pid);
                if (pg) {
                    int rw = table->schema->rowWidth();
                    int offset = PAGE_HEADER_SZ + s * rw;
                    char* zeroRow = new char[rw]();
                    pg->write(zeroRow, offset, rw);
                    delete[] zeroRow;
                    pool->releasePage(pid);
                }
                deleted++;
            }
            delete row;
        }
    }
    Logger::log("DELETE FROM %s: %d row(s) deleted.", tname, deleted);
}

// ─────────────────────────────────────────────────────────────────────────────
// SELECT * FROM t1, t2, t3 ON t1.c=t2.c AND t2.c=t3.c
// ─────────────────────────────────────────────────────────────────────────────
void QueryExecutor::execJoin(const Token* t, int n) {
    int fromIdx = -1;
    for (int i = 1; i < n; i++) if (t[i].type == TOK_FROM) { fromIdx = i; break; }
    if (fromIdx < 0) { Logger::error("JOIN: missing FROM."); return; }

    // Collect table names (identifiers separated by commas)
    char tableNames[MAX_GRAPH_NODES][64];
    int  tableCount = 0;
    int  i = fromIdx + 1;
    while (i < n && tableCount < MAX_GRAPH_NODES) {
        if (t[i].type == TOK_IDENT) {
            strncpy(tableNames[tableCount++], t[i].lexeme, 63);
            tableNames[tableCount-1][63] = '\0';
            i++;
            if (i < n && t[i].type == TOK_COMMA) i++;
        } else break;
    }

    // Find ON keyword
    int onIdx = -1;
    for (int j = fromIdx+1; j < n; j++)
        if (t[j].type == TOK_IDENT && strEqCI(t[j].lexeme, "ON")) { onIdx = j; break; }

    // Build join graph
    Graph g;
    for (int j = 0; j < tableCount; j++) {
        Table* tbl = catalog->getTable(tableNames[j]);
        int rc = tbl ? tbl->getRowCount() : 0;
        g.addNode(tableNames[j], rc);
    }

    // Parse ON conditions: tA.colA=tB.colB AND ...
    if (onIdx >= 0) {
        int j = onIdx + 1;
        while (j < n && t[j].type != TOK_EOF && t[j].type != TOK_SEMICOLON) {
            if (t[j].type == TOK_AND ||
                (t[j].type == TOK_IDENT && strEqCI(t[j].lexeme, "AND"))) {
                j++; continue;
            }
            // tA.colA
            char tA[64]={0}, cA[64]={0}, tB[64]={0}, cB[64]={0};
            const char* dotA = strchr(t[j].lexeme, '.');
            if (dotA) {
                int len = (int)(dotA - t[j].lexeme);
                if (len > 63) len = 63;
                strncpy(tA, t[j].lexeme, len);
                strncpy(cA, dotA+1, 63);
            } else { strncpy(tA, t[j].lexeme, 63); }
            j++;
            if (j < n && t[j].type == TOK_EQ) j++; // skip '='
            // tB.colB
            if (j < n) {
                const char* dotB = strchr(t[j].lexeme, '.');
                if (dotB) {
                    int len = (int)(dotB - t[j].lexeme);
                    if (len > 63) len = 63;
                    strncpy(tB, t[j].lexeme, len);
                    strncpy(cB, dotB+1, 63);
                } else { strncpy(tB, t[j].lexeme, 63); }
                j++;
            }
            g.addEdge(tA, cA, tB, cB);
        }
    }

    // Kruskal MST → optimal join order (Test Case C)
    KruskalMST mst;
    JoinPlan plan;
    int edgeCount = mst.run(g, plan);
    KruskalMST::printPlan(g, plan);
    Logger::log("Multi-table join routed via MST: %d join step(s).", edgeCount);

    // Execute nested-loop joins in MST order
    int totalJoined = 0;
    for (int e = 0; e < plan.joinCount; e++) {
        const GraphEdge& edge = plan.joins[e];
        const char* tnA = g.nodes[edge.nodeA].tableName;
        const char* tnB = g.nodes[edge.nodeB].tableName;
        Table* tA = catalog->getTable(tnA);
        Table* tB = catalog->getTable(tnB);
        if (!tA || !tB) continue;

        int idxA = tA->schema->indexOf(edge.colA);
        int idxB = tB->schema->indexOf(edge.colB);

        for (int pA = 0; pA < tA->numPages; pA++) {
            int pidA = tA->pageDirectory[pA];
            Page* pageA = pool->fetchPage(pidA);
            if (!pageA) continue;
            int rcA = pageA->getRowCount();
            pool->releasePage(pidA);

            for (int sA = 0; sA < rcA; sA++) {
                Row* rowA = tA->getRow(pidA, sA);
                if (!rowA) continue;

                for (int pB = 0; pB < tB->numPages; pB++) {
                    int pidB = tB->pageDirectory[pB];
                    Page* pageB = pool->fetchPage(pidB);
                    if (!pageB) continue;
                    int rcB = pageB->getRowCount();
                    pool->releasePage(pidB);

                    for (int sB = 0; sB < rcB; sB++) {
                        Row* rowB = tB->getRow(pidB, sB);
                        if (!rowB) continue;
                        bool match = false;
                        if (idxA >= 0 && idxB >= 0) {
                            const Field* fA = rowA->getField(idxA);
                            const Field* fB = rowB->getField(idxB);
                            if (fA && fB) match = (*fA == *fB);
                        }
                        if (match) {
                            rowA->print();
                            printf(" |JOIN| ");
                            rowB->print();
                            totalJoined++;
                        }
                        delete rowB;
                    }
                }
                delete rowA;
            }
        }
    }
    Logger::log("JOIN result: %d combined row(s).", totalJoined);
}
