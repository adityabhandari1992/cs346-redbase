//
// File:        ql_internal.h
// Description: Classes for all the operators in the QL component
// Authors:     Aditya Bhandari (adityasb@stanford.edu)
//

#ifndef QL_INTERNAL_H
#define QL_INTERNAL_H

#include <string>
#include <stdlib.h>
#include <memory>
#include "redbase.h"
#include "parser.h"
#include "printer.h"
#include "rm.h"
#include "ix.h"
#include "sm.h"
#include "ql.h"

// QL_Op
// QL Operator abstract class
class QL_Op {
public:
    virtual RC Open() = 0;
    virtual RC Close() = 0;
    virtual RC GetNext(char* recordData) = 0;
    virtual RC GetNext(RID &rid) { return QL_EOF; }
    virtual void Print(int indentationLevel) = 0;

    virtual void GetAttributeCount(int &attrCount) = 0;
    virtual void GetAttributeInfo(DataAttrInfo* attributes) = 0;
};


// QL_IndexScanOp
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

    void GetAttributeCount(int &attrCount);
    void GetAttributeInfo(DataAttrInfo* attributes);

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
    Value* v;
    int tupleLength;
    int attrCount;
    DataAttrInfo* attributes;
    int isOpen;
};


// QL_FileScanOp
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

    void GetAttributeCount(int &attrCount);
    void GetAttributeInfo(DataAttrInfo* attributes);

private:
    SM_Manager* smManager;
    RM_Manager* rmManager;
    RM_FileHandle rmFH;
    RM_FileScan rmFS;
    char relName[MAXNAME+1];
    char attrName[MAXNAME+1];
    bool cond;
    CompOp op;
    Value* v;
    int tupleLength;
    int attrCount;
    DataAttrInfo* attributes;
    int isOpen;
};


// QL_ProjectOp
// Project operator class
class QL_ProjectOp : public QL_Op {
public:
    QL_ProjectOp(SM_Manager* smManager, std::shared_ptr<QL_Op> childOp, int count, RelAttr relAttrs[]);
    ~QL_ProjectOp();

    RC Open();
    RC Close();
    RC GetNext(char* recordData);
    void Print(int indentationLevel);

    void GetAttributeCount(int &attrCount);
    void GetAttributeInfo(DataAttrInfo* attributes);

private:
    SM_Manager* smManager;
    std::shared_ptr<QL_Op> childOp;
    int relAttrCount;
    RelAttr* relAttrs;
    DataAttrInfo* attributes;
    int isOpen;
};

// QL_FilterOp
// Filter operator class
class QL_FilterOp : public QL_Op {
public:
    QL_FilterOp(SM_Manager* smManager, std::shared_ptr<QL_Op> childOp, Condition filterCond);
    ~QL_FilterOp();

    RC Open();
    RC Close();
    RC GetNext(char* recordData);
    void Print(int indentationLevel);

    void GetAttributeCount(int &attrCount);
    void GetAttributeInfo(DataAttrInfo* attributes);

private:
    SM_Manager* smManager;
    std::shared_ptr<QL_Op> childOp;
    Condition filterCond;
    int attrCount;
    DataAttrInfo* attributes;
    int isOpen;
};

// QL_CrossProductOp
// Cross product operator class
class QL_CrossProductOp : public QL_Op {
public:
    QL_CrossProductOp(SM_Manager* smManager, std::shared_ptr<QL_Op> leftOp, std::shared_ptr<QL_Op> rightOp);
    ~QL_CrossProductOp();

    RC Open();
    RC Close();
    RC GetNext(char* recordData);
    void Print(int indentationLevel);

    void GetAttributeCount(int &attrCount);
    void GetAttributeInfo(DataAttrInfo* attributes);

private:
    SM_Manager* smManager;
    std::shared_ptr<QL_Op> leftOp;
    std::shared_ptr<QL_Op> rightOp;
    int attrCount;
    DataAttrInfo* attributes;
    int firstTuple;
    char* leftData;
    char* rightData;
    int isOpen;
};

// QL_NLJoinOp
// Natural loop join operator class
class QL_NLJoinOp : public QL_Op {
public:
    QL_NLJoinOp(SM_Manager* smManager, std::shared_ptr<QL_Op> leftOp, std::shared_ptr<QL_Op> rightOp, Condition joinCond);
    ~QL_NLJoinOp();

    RC Open();
    RC Close();
    RC GetNext(char* recordData);
    void Print(int indentationLevel);

    void GetAttributeCount(int &attrCount);
    void GetAttributeInfo(DataAttrInfo* attributes);

private:
    SM_Manager* smManager;
    std::shared_ptr<QL_Op> leftOp;
    std::shared_ptr<QL_Op> rightOp;
    Condition joinCond;
    int attrCount;
    DataAttrInfo* attributes;
    int firstTuple;
    char* leftData;
    char* rightData;
    int isOpen;
};

// EX
// QL_ShuffleDataOp
// Operator for shuffling data across nodes
class QL_ShuffleDataOp {
public:
    QL_ShuffleDataOp(RM_Manager* rmManager, std::shared_ptr<QL_Op> childOp, int fromNode, int toNode);
    ~QL_ShuffleDataOp();

    RC Open();
    RC Close();
    RC GetData(RM_FileHandle &rmFH);
    void Print(int indentationLevel);

private:
    RM_Manager* rmManager;
    RM_FileHandle rmFH;
    std::shared_ptr<QL_Op> childOp;
    int tupleLength;
    int fromNode;
    int toNode;
    int isOpen;
};

// Helper methods
void PrintOperator(CompOp op);
void PrintValue(const Value* v);
RC GetAttrInfoFromArray(char* attributes, int attrCount, const char* relName, const char* attrName, char* attributeData);

template <typename T>
bool matchRecord(T lhsValue, T rhsValue, CompOp op);

#endif
