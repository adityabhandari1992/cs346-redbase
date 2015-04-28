//
// File:        ix_internal.h
// Description: Declarations internal to the IX component
// Authors:     Aditya Bhandari (adityasb@stanford.edu)
//

#ifndef IX_INTERNAL_H
#define IX_INTERNAL_H

#include <string>
#include "ix.h"

// Constants and defines
#define IX_NULL_POINTER -1
#define IX_NO_PAGE -1

const RID dummyRID(-1, -1);

// B+ Tree node types
enum IX_NodeType {
    ROOT,
    NODE,
    LEAF,
    ROOT_LEAF
};

// Value type in a B+ tree node
enum IX_ValueType {
    EMPTY,
    PAGE_ONLY,
    RID_FILLED
};

// Data Structures

// IX_NodeValue: Struct for the values stored in the B+ tree nodes
/* Stores the following:
    1) state - Current state of the node value - IX_ValueType
    2) rid - RID of a record - RID
    3) pageNumber - Page number of the next page - PageNum
*/
struct IX_NodeValue {
    IX_ValueType state;
    RID rid;
    PageNum page;

    IX_NodeValue() {
        this->state = EMPTY;
        this->rid = dummyRID;
        this->page = IX_NO_PAGE;
    }
};

const IX_NodeValue dummyNodeValue;

// IX_NodeHeader: Struct for the index node header
/* Stores the following:
    1) numberKeys - Number of keys in the node - integer
    2) keyCapacity - Number of keys the node can accommodate - integer
    3) type - Type of the node - IX_NodeType
    4) parent - Parent node - PageNum
    5) left - Left sibling node - PageNum
*/
struct IX_NodeHeader {
    int numberKeys;
    int keyCapacity;
    IX_NodeType type;
    PageNum parent;
    PageNum left;
};

// IX_BucketPageHeader: Struct for the index bucket page header
/* Stores the following:
    1) numberRecords - Number of records in the bucket - integer
    2) recordCapacity - Maximum number of records - integer
    3) parentNode - Page number of the parent node - PageNum
    4) nextBucket - Page number of the next chained bucket - PageNum
*/
struct IX_BucketPageHeader {
    int numberRecords;
    int recordCapacity;
    PageNum parentNode;
    // PageNum nextBucket;
};

#endif