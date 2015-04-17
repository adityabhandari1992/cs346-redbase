//
// ix.h
//
//   Index Manager Component Interface
//

#ifndef IX_H
#define IX_H

// Please do not include any other files than the ones below in this file.

#include "redbase.h"  // Please don't change these lines
#include "rm_rid.h"  // Please don't change these lines
#include "pf.h"

//
// IX_IndexHandle: IX Index File interface
//
class IX_IndexHandle {
public:
    IX_IndexHandle();
    ~IX_IndexHandle();

    // Insert a new index entry
    RC InsertEntry(void *pData, const RID &rid);

    // Delete a new index entry
    RC DeleteEntry(void *pData, const RID &rid);

    // Force index files to disk
    RC ForcePages();
};

//
// IX_IndexScan: condition-based scan of index entries
//
class IX_IndexScan {
public:
    IX_IndexScan();
    ~IX_IndexScan();

    // Open index scan
    RC OpenScan(const IX_IndexHandle &indexHandle,
                CompOp compOp,
                void *value,
                ClientHint  pinHint = NO_HINT);

    // Get the next matching entry return IX_EOF if no more matching
    // entries.
    RC GetNextEntry(RID &rid);

    // Close index scan
    RC CloseScan();
};

//
// IX_Manager: provides IX index file management
//
class IX_Manager {
public:
    IX_Manager(PF_Manager &pfm);
    ~IX_Manager();

    // Create a new Index
    RC CreateIndex(const char *fileName, int indexNo,
                   AttrType attrType, int attrLength);

    // Destroy an Index
    RC DestroyIndex(const char *fileName, int indexNo);

    // Open an Index
    RC OpenIndex(const char *fileName, int indexNo,
                 IX_IndexHandle &indexHandle);

    // Close an Index
    RC CloseIndex(IX_IndexHandle &indexHandle);

private:
    PF_Manager* pfManager;      // PF_Manager object
};

//
// Print-error function
//
void IX_PrintError(RC rc);

// Warnings
#define IX_LARGE_RECORD             (START_IX_WARN + 0) // Record size is too large
#define IX_SMALL_RECORD             (START_IX_WARN + 1) // Record size is too small
#define IX_LASTWARN                 IX_SMALL_RECORD

// Errors
#define IX_INVALIDNAME          (START_IX_ERR - 0) // Invalid PC file name
#define IX_INCONSISTENT_BITMAP  (START_IX_ERR - 1) // Inconsistent bitmap in page

// Error in UNIX system call or library routine
#define IX_UNIX            (START_IX_ERR - 2) // Unix error
#define IX_LASTERROR       IX_UNIX

#endif
