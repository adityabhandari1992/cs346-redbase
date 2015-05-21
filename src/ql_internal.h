//
// File:        ql_internal.h
// Description: Declarations internal to the QL component
// Authors:     Aditya Bhandari (adityasb@stanford.edu)
//

#ifndef QL_INTERNAL_H
#define QL_INTERNAL_H

#include <string>
#include <stdlib.h>
#include "redbase.h"
#include "parser.h"
#include "printer.h"
#include "rm.h"
#include "ix.h"
#include "sm.h"
#include "ql.h"

// QL_Op abstract class
class QL_Op {
public:
    virtual RC Open() = 0;
    virtual RC Close() = 0;
    virtual RC GetNext(char* recordData) = 0;
    virtual RC GetNext(RID &rid) { return QL_EOF; }
    virtual void Print(int indentationLevel) = 0;
};

// Index Scan operator class
class QL_IndexScanOp : public QL_Op {
public:
    QL_IndexScanOp(SM_Manager* smManager, IX_Manager* ixManager, RM_Manager* rmManager,
                   const char* relName, char* attrName, CompOp op, const Value* v);
    ~QL_IndexScanOp();

    RC Open();
    RC Close();
    RC GetNext(char* recordData);
    RC GetNext(RID &rid);
    void Print(int indentationLevel);

private:
    SM_Manager* smManager;
    IX_Manager* ixManager;
    RM_Manager* rmManager;
    IX_IndexHandle ixIH;
    IX_IndexScan ixIS;
    RM_FileHandle rmFH;
    char relName[MAXNAME+1];
    char attrName[MAXNAME+1];
    CompOp op;
    const Value* v;
    int tupleLength;
    int attrCount;

    int isOpen;
};

// File Scan operator class
class QL_FileScanOp : public QL_Op {
public:
    QL_FileScanOp(SM_Manager* smManager, RM_Manager* rmManager, const char* relName,
                  bool cond, char* attrName, CompOp op, const Value* v);
    ~QL_FileScanOp();

    RC Open();
    RC Close();
    RC GetNext(char* recordData);
    RC GetNext(RID &rid);
    void Print(int indentationLevel);

private:
    SM_Manager* smManager;
    RM_Manager* rmManager;
    RM_FileHandle rmFH;
    RM_FileScan rmFS;
    char relName[MAXNAME+1];
    char attrName[MAXNAME+1];
    bool cond;
    CompOp op;
    const Value* v;
    int tupleLength;
    int attrCount;

    int isOpen;
};

// Helper methods
void PrintOperator(CompOp op);
void PrintValue(const Value* v);
RC GetAttrInfoFromArray(char* attributes, int attrCount, const char* attrName, char* attributeData);

#endif
