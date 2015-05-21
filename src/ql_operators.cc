//
// File:        ql_operators.cc
// Description: QL_Op and derived classes implementation
// Authors:     Aditya Bhandari (adityasb@stanford.edu)
//

#include <cstdio>
#include <iostream>
#include <unistd.h>
#include <memory>
#include "redbase.h"
#include "ql.h"
#include "ql_internal.h"
#include "sm.h"
#include "ix.h"
#include "rm.h"
#include "printer.h"
#include "parser.h"

using namespace std;


/********** QL_IndexScanOp class **********/

// Constructor
QL_IndexScanOp::QL_IndexScanOp(SM_Manager* smManager, IX_Manager* ixManager, RM_Manager* rmManager,
                               const char* relName, char* attrName, CompOp op, const Value* v) {
    // Store the objects
    this->smManager = smManager;
    this->ixManager = ixManager;
    this->rmManager = rmManager;
    memset(this->relName, 0, MAXNAME+1);
    strcpy(this->relName, relName);
    memset(this->attrName, 0, MAXNAME+1);
    strcpy(this->attrName, attrName);
    this->op = op;
    this->v = v;

    // Get the relation information
    SM_RelcatRecord* rcRecord = new SM_RelcatRecord;
    memset(rcRecord, 0, sizeof(SM_RelcatRecord));
    smManager->GetRelInfo(relName, rcRecord);
    attrCount = rcRecord->attrCount;
    tupleLength = rcRecord->tupleLength;
    delete rcRecord;

    // Create the attributes array
    attributes = new DataAttrInfo[attrCount];
    smManager->GetAttrInfo(relName, attrCount, (char*) attributes);

    // Set open flag to FALSE
    isOpen = FALSE;
}

// Destructor
QL_IndexScanOp::~QL_IndexScanOp() {
    // Delete the attrributes array
    delete[] attributes;
}

// Open the operator
RC QL_IndexScanOp::Open() {
    // Check if already open
    if (isOpen) {
        return QL_OPERATOR_OPEN;
    }

    // Get the index attribute information
    int rc;
    DataAttrInfo* attributeData = new DataAttrInfo;
    if ((rc = GetAttrInfoFromArray((char*) attributes, attrCount, attrName, (char*) attributeData))) {
        return rc;
    }
    int attrIndexNo = attributeData->indexNo;

    // Open the RM file
    if ((rc = rmManager->OpenFile(relName, rmFH))) {
        return rc;
    }

    // Open the index handle and index scan
    if ((rc = ixManager->OpenIndex(relName, attrIndexNo, ixIH))) {
        return rc;
    }
    if ((rc = ixIS.OpenScan(ixIH, op, v->data))) {
        return rc;
    }

    // Set the flag
    isOpen = TRUE;

    // Clean up
    delete attributeData;

    return OK_RC;
}

// Close the operator
RC QL_IndexScanOp::Close() {
    // Check if already closed
    if (!isOpen) {
        return QL_OPERATOR_CLOSED;
    }

    int rc;

    // Close the index file and scan
    if ((rc = ixIS.CloseScan())) {
        return rc;
    }
    if ((rc = ixManager->CloseIndex(ixIH))) {
        return rc;
    }

    // Close the RM file
    if ((rc = rmManager->CloseFile(rmFH))) {
        return rc;
    }

    // Set the flag
    isOpen = FALSE;

    return OK_RC;
}

// Get the next data
RC QL_IndexScanOp::GetNext(char* recordData) {
    // Check if closed
    if (!isOpen) {
        return QL_OPERATOR_CLOSED;
    }

    int rc;
    RID rid;
    RM_Record rec;
    char* data;

    // Get the next record from the scan
    rc = ixIS.GetNextEntry(rid);
    if (rc == IX_EOF) {
        return QL_EOF;
    }
    else if (rc) {
        return rc;
    }
    else {
        // Get the record from the file
        if ((rc = rmFH.GetRec(rid, rec))) {
            return rc;
        }
        if ((rc = rec.GetData(data))) {
            return rc;
        }
    }

    // Copy the data to the return parameter
    memcpy(recordData, data, tupleLength);

    return OK_RC;
}

// Get the next data and RID
RC QL_IndexScanOp::GetNext(RID &rid) {
    // Check if closed
    if (!isOpen) {
        return QL_OPERATOR_CLOSED;
    }

    int rc;
    RM_Record rec;

    // Get the next record from the scan
    rc = ixIS.GetNextEntry(rid);
    if (rc == IX_EOF) {
        return QL_EOF;
    }
    else if (rc) {
        return rc;
    }

    return OK_RC;
}

// Get the attribute count
void QL_IndexScanOp::GetAttributeCount(int &attrCount) {
    attrCount = this->attrCount;
}

// Get the attribute information
void QL_IndexScanOp::GetAttributeInfo(DataAttrInfo* attributes) {
    for (int i=0; i<attrCount; i++) {
        attributes[i] = this->attributes[i];
    }
}

// Print the physical query plan
void QL_IndexScanOp::Print(int indentationLevel) {
    for (int i=0; i<indentationLevel; i++) {
        cout << "\t";
    }
    cout << "IndexScanOp (";
    cout << relName << ", " << attrName;
    PrintOperator(op);
    PrintValue(v);
    cout << ")" << endl;
}


/********** QL_FileScanOp class **********/

// Constructor
QL_FileScanOp::QL_FileScanOp(SM_Manager* smManager, RM_Manager* rmManager, const char* relName,
                             bool cond, char* attrName, CompOp op, const Value* v) {
    // Store the objects
    this->smManager = smManager;
    this->rmManager = rmManager;
    memset(this->relName, 0, MAXNAME+1);
    strcpy(this->relName, relName);
    this->cond = cond;
    if (cond) {
        memset(this->attrName, 0, MAXNAME+1);
        strcpy(this->attrName, attrName);
        this->op = op;
        this->v = v;
    }

    // Get the relation information
    SM_RelcatRecord* rcRecord = new SM_RelcatRecord;
    memset(rcRecord, 0, sizeof(SM_RelcatRecord));
    smManager->GetRelInfo(relName, rcRecord);
    attrCount = rcRecord->attrCount;
    tupleLength = rcRecord->tupleLength;
    delete rcRecord;

    // Create the attributes array
    attributes = new DataAttrInfo[attrCount];
    smManager->GetAttrInfo(relName, attrCount, (char*) attributes);

    // Set open flag to FALSE
    isOpen = FALSE;
}

// Destructor
QL_FileScanOp::~QL_FileScanOp() {
    // Delete the attributes array
    delete[] attributes;
}

// Open the operator
RC QL_FileScanOp::Open() {
    // Check if already open
    if (isOpen) {
        return QL_OPERATOR_OPEN;
    }

    // Open the RM file
    int rc;
    if ((rc = rmManager->OpenFile(relName, rmFH))) {
        return rc;
    }

    // Open the RM file scan
    if (cond) {
        // Get the scan attribute information
        DataAttrInfo* attributeData = new DataAttrInfo;
        if ((rc = GetAttrInfoFromArray((char*) attributes, attrCount, attrName, (char*) attributeData))) {
            return rc;
        }
        if ((rc = rmFS.OpenScan(rmFH, attributeData->attrType, attributeData->attrLength, attributeData->offset, op, v->data))) {
            return rc;
        }
        delete attributeData;
    }
    else {
        if ((rc = rmFS.OpenScan(rmFH, INT, 4, 0, NO_OP, NULL))) {
            return rc;
        }
    }

    // Set the flag
    isOpen = TRUE;

    return OK_RC;
}

// Close the operator
RC QL_FileScanOp::Close() {
    // Check if already closed
    if (!isOpen) {
        return QL_OPERATOR_CLOSED;
    }

    int rc;

    // Close the file and scan
    if ((rc = rmFS.CloseScan())) {
        return rc;
    }
    if ((rc = rmManager->CloseFile(rmFH))) {
        return rc;
    }

    // Set the flag
    isOpen = FALSE;

    return OK_RC;
}

// Get the next data
RC QL_FileScanOp::GetNext(char* recordData) {
    // Check if closed
    if (!isOpen) {
        return QL_OPERATOR_CLOSED;
    }

    int rc;
    RID rid;
    RM_Record rec;
    char* data;

    // Get the next record from the scan
    rc = rmFS.GetNextRec(rec);
    if (rc == RM_EOF) {
        return QL_EOF;
    }
    else if (rc) {
        return rc;
    }
    else {
        // Get the record data
        if ((rc = rec.GetData(data))) {
            return rc;
        }
    }

    // Copy the data to the return parameter
    memcpy(recordData, data, tupleLength);

    return OK_RC;
}

// Get the next data and RID
RC QL_FileScanOp::GetNext(RID &rid) {
    // Check if closed
    if (!isOpen) {
        return QL_OPERATOR_CLOSED;
    }

    int rc;
    RM_Record rec;

    // Get the next record from the scan
    rc = rmFS.GetNextRec(rec);
    if (rc == RM_EOF) {
        return QL_EOF;
    }
    else if (rc) {
        return rc;
    }
    else {
        // Get the RID from the record
        if ((rc = rec.GetRid(rid))) {
            return rc;
        }
    }

    return OK_RC;
}

// Get the attribute count
void QL_FileScanOp::GetAttributeCount(int &attrCount) {
    attrCount = this->attrCount;
}

// Get the attribute information
void QL_FileScanOp::GetAttributeInfo(DataAttrInfo* attributes) {
    for (int i=0; i<attrCount; i++) {
        attributes[i] = this->attributes[i];
    }
}

// Print the physical query plan
void QL_FileScanOp::Print(int indentationLevel) {
    for (int i=0; i<indentationLevel; i++) {
        cout << "\t";
    }
    cout << "FileScanOp (";
    cout << relName;
    if (cond) {
        cout << ", " << attrName;
        PrintOperator(op);
        PrintValue(v);
    }
    cout << ")" << endl;
}


/********** QL_ProjectOp class **********/

// Constructor
QL_ProjectOp::QL_ProjectOp(SM_Manager* smManager, shared_ptr<QL_Op> childOp, int count, RelAttr relAttrs[]) {
    // Store the objects
    this->smManager = smManager;
    this->childOp = childOp;
    this->relAttrCount = count;

    // Copy the relAttrs array
    this->relAttrs = new RelAttr[count];
    for (int i=0; i<count; i++) {
        if (relAttrs[i].relName != NULL) this->relAttrs[i].relName = relAttrs[i].relName;
        this->relAttrs[i].attrName = relAttrs[i].attrName;
    }

    // Create the attributes array
    attributes = new DataAttrInfo[count];
    SM_AttrcatRecord* acRecord = new SM_AttrcatRecord;
    int currentOffset = 0;
    for (int i=0; i<count; i++) {
        smManager->GetAttrInfo(relAttrs[i].relName, relAttrs[i].attrName, acRecord);
        if (relAttrs[i].relName != NULL) strcpy(attributes[i].relName, relAttrs[i].relName);
        strcpy(attributes[i].attrName, relAttrs[i].attrName);
        attributes[i].offset = currentOffset;
        attributes[i].attrType = acRecord->attrType;
        attributes[i].attrLength = acRecord->attrLength;
        attributes[i].indexNo = -1;
        currentOffset += attributes[i].attrLength;
    }
    delete acRecord;

    // Set open flag to FALSE
    isOpen = FALSE;
}

// Destructor
QL_ProjectOp::~QL_ProjectOp() {
    // Delete the relAttrs and attributes array
    delete[] relAttrs;
    delete[] attributes;
}

// Open the operator
RC QL_ProjectOp::Open() {
    // Check if already open
    if (isOpen) {
        return QL_OPERATOR_OPEN;
    }

    // Open the child operator
    int rc;
    if ((rc = childOp->Open())) {
        return rc;
    }

    // Set the flag
    isOpen = TRUE;

    return OK_RC;
}

// Close the operator
RC QL_ProjectOp::Close() {
    // Check if already closed
    if (!isOpen) {
        return QL_OPERATOR_CLOSED;
    }

    // Close the child operator
    int rc;
    if ((rc = childOp->Close())) {
        return rc;
    }

    // Set the flag
    isOpen = FALSE;

    return OK_RC;
}

// Get the next data
RC QL_ProjectOp::GetNext(char* recordData) {
    // Check if closed
    if (!isOpen) {
        return QL_OPERATOR_CLOSED;
    }

    // Get the child attribute information
    int rc;
    int childAttrCount;
    childOp->GetAttributeCount(childAttrCount);
    DataAttrInfo* childAttributes = new DataAttrInfo[childAttrCount];
    childOp->GetAttributeInfo(childAttributes);
    int tupleLength = 0;
    for (int i=0; i<childAttrCount; i++) {
        tupleLength += childAttributes[i].attrLength;
    }
    char* data = new char[tupleLength];

    // Get the next record data from the child
    if ((rc = childOp->GetNext(data))) {
        return rc;
    }

    // Create the new tuple for the required attributes
    DataAttrInfo* originalAttrData = new DataAttrInfo;
    for (int i=0; i<relAttrCount; i++) {
        if ((rc = GetAttrInfoFromArray((char*) childAttributes, childAttrCount, attributes[i].attrName, (char*) originalAttrData))) {
            return rc;
        }
        memcpy(recordData + attributes[i].offset, data + originalAttrData->offset, attributes[i].attrLength);
    }
    delete originalAttrData;
    delete[] data;

    return OK_RC;
}

// Get the attribute count
void QL_ProjectOp::GetAttributeCount(int &attrCount) {
    attrCount = this->relAttrCount;
}

// Get the attribute information
void QL_ProjectOp::GetAttributeInfo(DataAttrInfo* attributes) {
    for (int i=0; i<relAttrCount; i++) {
        attributes[i] = this->attributes[i];
    }
}

// Print the physical query plan
void QL_ProjectOp::Print(int indentationLevel) {
    for (int i=0; i<indentationLevel; i++) {
        cout << "\t";
    }
    cout << "ProjectOp (";
    for (int i=0; i<relAttrCount; i++) {
        if (relAttrs[i].relName != NULL) cout << relAttrs[i].relName << ".";
        cout << relAttrs[i].attrName;
        if (i != relAttrCount-1) cout << ", ";
    }
    cout << ")" << endl;
}


/********** Helper Methods **********/

// Get the string form of the operator
void PrintOperator(CompOp op) {
    switch(op) {
        case EQ_OP:
            cout << " = ";
            break;
        case LT_OP:
            cout << " < ";
            break;
        case GT_OP:
            cout << " > ";
            break;
        case LE_OP:
            cout << " <= ";
            break;
        case GE_OP:
            cout << " >= ";
            break;
        case NE_OP:
            cout << " != ";
            break;
        default:
            cout << " NO_OP ";
    }
}

// Get the value in a string for printing
void PrintValue(const Value* v) {
    AttrType vType = v->type;
    if (vType == INT) {
        int value = *static_cast<int*>(v->data);
        cout << value;
    }
    else if (vType == FLOAT) {
        float value = *static_cast<float*>(v->data);
        cout << value;
    }
    else {
        char* value = static_cast<char*>(v->data);
        cout << value;
    }
}

// Get the attribute info from the attributes array
RC GetAttrInfoFromArray(char* attributes, int attrCount, const char* attrName, char* attributeData) {
    bool found = false;
    DataAttrInfo* attributesArray = (DataAttrInfo*) attributes;
    DataAttrInfo* attribute = (DataAttrInfo*) attributeData;
    for (int i=0; i<attrCount; i++) {
        if (strcmp(attributesArray[i].attrName, attrName) == 0) {
            strcpy(attribute->relName, attributesArray[i].relName);
            strcpy(attribute->attrName, attributesArray[i].attrName);
            attribute->offset = attributesArray[i].offset;
            attribute->attrType = attributesArray[i].attrType;
            attribute->attrLength = attributesArray[i].attrLength;
            attribute->indexNo = attributesArray[i].indexNo;
            found = true;
            break;
        }
    }

    if (!found) {
        return QL_ATTRIBUTE_NOT_FOUND;
    }

    return OK_RC;
}
