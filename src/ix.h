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

// IX_IndexHeader: Struct for the index file header
/* Stores the following:
    1) attrType - Attribute type for the index - AttrType
    2) attrLength - Attribute length - integer
    3) rootPage - Page number of the B+ Tree root - PageNum
    4) degree - Degree of a node in the B+ Tree - integer
*/
struct IX_IndexHeader {
    AttrType attrType;
    int attrLength;
    PageNum rootPage;
    int degree;
};

struct IX_Entry {
    void* keyValue;
    RID rid;
};

//
// IX_IndexHandle: IX Index File interface
//
class IX_IndexHandle {
    friend class IX_Manager;
    friend class IX_IndexScan;
public:
    IX_IndexHandle();
    ~IX_IndexHandle();

    // Insert a new index entry
    RC InsertEntry(void *pData, const RID &rid);

    // Delete a new index entry
    RC DeleteEntry(void *pData, const RID &rid);

    // Force index files to disk
    RC ForcePages();

private:
    PF_FileHandle pfFH;                 // PF file handle
    IX_IndexHeader indexHeader;         // Index file header
    int isOpen;                         // index handle open flag
    int headerModified;                 // Modified flag for the index header
    IX_Entry lastDeletedEntry;               // Last deleted entry

    RC InsertEntryRecursive(void *pData, const RID &rid, PageNum node);
    RC pushKeyUp(void* pData, PageNum node, PageNum left, PageNum right);

    RC SearchEntry(void* pData, PageNum node, PageNum &pageNumber);
    RC DeleteFromLeaf(void* pData, const RID &rid, PageNum node);
    RC pushDeletionUp(PageNum node, PageNum child);
    bool compareRIDs(const RID &rid1, const RID &rid2);

    template<typename T>
    bool satisfiesInterval(T key1, T key2, T value);

    // template<typename T>
    // RC InsertInRootLeaf(void* pData, RID &rid, char* keyData, char* valueData, int numberKeys, int keyCapacity);
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

private:
    PageNum pageNumber;                     // Current page number
    int keyPosition;                        // Current key position
    int bucketPosition;                     // Current bucket position
    const IX_IndexHandle* indexHandle;      // Index handle for the index
    AttrType attrType;                      // Attribute type
    int attrLength;                         // Attribute length
    CompOp compOp;                          // Comparison operator
    void* value;                            // Value to be compared
    ClientHint pinHint;                     // Pinning hint
    int scanOpen;                           // Flag to track if scan open
    int degree;                             // Degree of the nodes
    int inBucket;                           // Flag whether currently in bucket
    IX_Entry lastScannedEntry;                   // Last scanned entry

    RC SearchEntry(PageNum node, PageNum &pageNumber, int &keyPosition);

    template<typename T>
    bool satisfiesCondition(T key, T value);
    template<typename T>
    bool satisfiesInterval(T key1, T key2, T value);
    bool compareRIDs(const RID &rid1, const RID &rid2);
    bool compareEntries(const IX_Entry &e1, const IX_Entry &e2);
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

    std::string generateIndexFileName(const char* fileName, int indexNo);
    int findDegreeOfNode(int attrLength);
};

//
// Print-error function
//
void IX_PrintError(RC rc);

// Warnings
#define IX_NEGATIVE_INDEX           (START_IX_WARN + 0) // Index number is negative
#define IX_INCONSISTENT_ATTRIBUTE   (START_IX_WARN + 1) // Index attribute is inconsistent
#define IX_INDEX_OPEN               (START_IX_WARN + 2) // Index file is open
#define IX_INDEX_CLOSED             (START_IX_WARN + 3) // Index file is closed
#define IX_INCONSISTENT_NODE        (START_IX_WARN + 4) // Index node is invalid
#define IX_KEY_NOT_FOUND            (START_IX_WARN + 5) // Index key was not found
#define IX_NULL_ENTRY               (START_IX_WARN + 6) // Null index entry
#define IX_ENTRY_EXISTS             (START_IX_WARN + 7) // Index entry exists
#define IX_BUCKET_FULL              (START_IX_WARN + 8) // Bucket full
#define IX_EOF                      (START_IX_WARN + 9) // End of file
#define IX_NULL_FILENAME            (START_IX_WARN + 10) // Null file name
#define IX_INVALID_ATTRIBUTE        (START_IX_WARN + 11) // Invalid attribute
#define IX_INVALID_OPERATOR         (START_IX_WARN + 12) // Invalid operator
#define IX_SCAN_CLOSED              (START_IX_WARN + 13) // Scan is closed
#define IX_DELETE_ENTRY_NOT_FOUND   (START_IX_WARN + 14) // Delete an entry that does not exist
#define IX_LASTWARN                 IX_DELETE_ENTRY_NOT_FOUND

// Errors
#define IX_INVALIDNAME          (START_IX_ERR - 0) // Invalid PC file name

// Error in UNIX system call or library routine
#define IX_UNIX            (START_IX_ERR - 2) // Unix error
#define IX_LASTERROR       IX_UNIX

#endif
