//
// File:        ix_indexhandle.cc
// Description: IX_IndexHandle class implementation
// Authors:     Aditya Bhandari (adityasb@stanford.edu)
//

#include "ix_internal.h"
#include "ix.h"
#include <cstring>
#include <iostream>
using namespace std;

// Constructor
IX_IndexHandle::IX_IndexHandle() {
    // Set the flags
    isOpen = FALSE;
    headerModified = FALSE;

    // Initialize the index header
    indexHeader.rootPage = IX_NO_PAGE;
}

// Destructor
IX_IndexHandle::~IX_IndexHandle() {
    // Nothing to free
}

// Method: InsertEntry(void *pData, const RID &rid)
// Insert a new index entry
/* Steps:
    1) Check if the index handle is open
    2) Check that the pData is not null
    3) Get the root node from the index header
    4) If root node is IX_NO_PAGE
        - Allocate a new page for the root node
        - Initialize the node header and copy to the page
        - Insert the pData, RID in the node
        - Unpin the page
        - Return OK
    5) Else if root node exists
        - Check type of node
    6) If type is ROOT_LEAF
        - If pData exists in the node, check if RID is same
            - If not same, go to bucket page (allocate if not present)
            - Insert RID in the bucket page
        - Else if pData does not exist
            - If number of keys < capacity, insert new entry in the node
            - If number of keys = capacity
                - Split the node into 2 nodes (allocate new page)
                - Change types of both to LEAF
                - Allocate new root page and set type to ROOT
                - Copy the first key from right node to root and set page pointers
                - Update the root page in the index header
                - Unpin all pages
    7) Else if type is ROOT
        - Call recursive insert function with root page
*/
RC IX_IndexHandle::InsertEntry(void *pData, const RID &rid) {
    // Check if the index handle is open
    if (!isOpen) {
        return IX_INDEX_CLOSED;
    }

    // Check if pData is null
    if (pData == NULL) {
        return IX_NULL_ENTRY;
    }

    // Declare an integer for the return code
    int rc;

    // Get the root node from the index header
    PageNum rootPage = indexHeader.rootPage;
    int degree = indexHeader.degree;
    int attrLength = indexHeader.attrLength;
    AttrType attrType = indexHeader.attrType;

    // If root node does not exist
    if (rootPage == IX_NO_PAGE) {
        // Allocate a new root page
        PF_PageHandle pfPH;
        if ((rc = pfFH.AllocatePage(pfPH))) {
            return rc;
        }

        // Get the page data
        char* pageData;
        if ((rc = pfPH.GetData(pageData))) {
            return rc;
        }
        PageNum pageNumber;
        if ((rc = pfPH.GetPageNum(pageNumber))) {
            return rc;
        }
        if ((rc = pfFH.MarkDirty(pageNumber))) {
            return rc;
        }

        // Initialize a node header
        IX_NodeHeader* rootHeader = new IX_NodeHeader;
        rootHeader->numberKeys = 1;
        rootHeader->keyCapacity = degree;
        rootHeader->type = ROOT_LEAF;
        rootHeader->parent = IX_NO_PAGE;

        // Copy the node header to the page
        memcpy(pageData, (char*) rootHeader, sizeof(IX_NodeHeader));
        delete rootHeader;

        // Allocate a NodeValue array to the node
        IX_NodeValue* valueArray = new IX_NodeValue[degree+1];
        valueArray[0].state = RID_FILLED;
        valueArray[0].rid = rid;
        valueArray[0].page = IX_NO_PAGE;
        int valueOffset = sizeof(IX_NodeHeader) + degree*attrLength;
        memcpy(pageData+valueOffset, (char*) valueArray, sizeof(IX_NodeValue)*(degree+1));
        delete[] valueArray;

        // Allocate a key array to the node
        if (attrType == INT) {
            int* keyArray = new int[degree];
            int givenKey = *static_cast<int*>(pData);
            keyArray[0] = givenKey;
            memcpy(pageData+sizeof(IX_NodeHeader), keyArray, attrLength*degree);
            delete[] keyArray;
        }
        else if (attrType == FLOAT) {
            float* keyArray = new float[degree];
            float givenKey = *static_cast<float*>(pData);
            keyArray[0] = givenKey;
            memcpy(pageData+sizeof(IX_NodeHeader), keyArray, attrLength*degree);
            delete[] keyArray;
        }
        else {
            string* keyArray = new string[degree];
            char* givenKeyChar = static_cast<char*>(pData);
            string givenKey = "";
            for (int i=0; i < attrLength; i++) {
                givenKey += givenKeyChar[i];
            }
            keyArray[0] = givenKey;
            memcpy(pageData+sizeof(IX_NodeHeader), keyArray, attrLength*degree);
            delete[] keyArray;
        }

        // Update the root page in the index header
        indexHeader.rootPage = pageNumber;
        headerModified = TRUE;

        // Unpin the page
        if ((rc = pfFH.UnpinPage(pageNumber))) {
            return rc;
        }

        // Return OK
        return OK_RC;
    }

    // Else if root node exists
    else {
        // Get the page data
        PF_PageHandle pfPH;
        char* pageData;
        if ((rc = pfFH.GetThisPage(rootPage, pfPH))) {
            return rc;
        }
        if ((rc = pfPH.GetData(pageData))) {
            return rc;
        }
        if ((rc = pfFH.MarkDirty(rootPage))) {
            return rc;
        }

        // Check type of node
        IX_NodeHeader* nodeHeader = (IX_NodeHeader*) pageData;
        IX_NodeType type = nodeHeader->type;
        int numberKeys = nodeHeader->numberKeys;
        int keyCapacity = nodeHeader->keyCapacity;
        char* keyData = pageData + sizeof(IX_NodeHeader);
        char* valueData = keyData + attrLength*degree;
        IX_NodeValue* valueArray = (IX_NodeValue*) valueData;

        // If the type is ROOT_LEAF
        if (type == ROOT_LEAF) {
            if (attrType == INT) {

                // Check if pData is already a key
                int* keyArray = (int*) keyData;
                int givenKey = *static_cast<int*>(pData);
                int index = -1;
                for (int i=0; i<numberKeys; i++) {
                    if (keyArray[i] == givenKey) {
                        index = i;
                        break;
                    }
                }

                // If key exists, check if the same RID exists
                if (index != -1) {
                    IX_NodeValue value = valueArray[index];
                    if (compareRIDs(value.rid, rid)) {
                        return IX_ENTRY_EXISTS;
                    }
                    else {
                        // Get the bucket page
                        PageNum bucketPage = value.page;
                        if (bucketPage == IX_NO_PAGE) {
                            // Allocate a new bucket page
                            PF_PageHandle bucketPFPH;
                            if ((rc = pfFH.AllocatePage(bucketPFPH))) {
                                return rc;
                            }
                            if ((rc = bucketPFPH.GetPageNum(bucketPage))) {
                                return rc;
                            }
                            valueArray[index].page = bucketPage;
                            memcpy(valueData, (char*) valueArray, sizeof(IX_NodeValue)*(degree+1));

                            // Initialize and copy the bucket header
                            IX_BucketPageHeader* bucketHeader = new IX_BucketPageHeader;
                            bucketHeader->numberRecords = 1;
                            int recordCapacity = (PF_PAGE_SIZE-sizeof(IX_BucketPageHeader)) / (sizeof(RID));
                            bucketHeader->recordCapacity = recordCapacity;
                            // bucketHeader->nextBucket = IX_NO_PAGE;

                            char* bucketData;
                            if ((rc = bucketPFPH.GetData(bucketData))) {
                                return rc;
                            }
                            if ((rc = pfFH.MarkDirty(bucketPage))) {
                                return rc;
                            }
                            memcpy(bucketData, (char*) bucketHeader, sizeof(IX_BucketPageHeader));
                            delete bucketHeader;

                            // Copy the RID to the bucket
                            RID* ridList = new RID[recordCapacity];
                            ridList[0] = rid;
                            memcpy(bucketData+sizeof(IX_BucketPageHeader), ridList, sizeof(RID)*recordCapacity);

                            // Unpin the bucket page
                            if ((rc = pfFH.UnpinPage(bucketPage))) {
                                return rc;
                            }
                        }
                        else {
                            // Get the data from the bucket page
                            PF_PageHandle bucketPFPH;
                            char* bucketData;
                            if ((rc = pfFH.GetThisPage(bucketPage, bucketPFPH))) {
                                return rc;
                            }
                            if ((rc = bucketPFPH.GetData(bucketData))) {
                                return rc;
                            }
                            if ((rc = pfFH.MarkDirty(bucketPage))) {
                                return rc;
                            }

                            // Get the bucket page header
                            IX_BucketPageHeader* bucketHeader = (IX_BucketPageHeader*) bucketData;
                            int numberRecords = bucketHeader->numberRecords;
                            int recordCapacity = bucketHeader->recordCapacity;

                            // Check the entries in the bucket for the same rid
                            char* ridData = bucketData + sizeof(IX_BucketPageHeader);
                            RID* ridList = (RID*) ridData;
                            for (int i=0; i<numberRecords; i++) {
                                if (compareRIDs(ridList[i],rid)) {
                                    return IX_ENTRY_EXISTS;
                                }
                            }

                            // Insert the new RID if not found
                            if (numberRecords == recordCapacity) {
                                return IX_BUCKET_FULL;
                            }
                            ridList[numberRecords] = rid;
                            memcpy(ridData, (char*) ridList, sizeof(sizeof(RID)*recordCapacity));

                            bucketHeader->numberRecords++;
                            memcpy(bucketData, (char*) bucketHeader, sizeof(IX_BucketPageHeader));

                            // Unpin the bucket page
                            if ((rc = pfFH.UnpinPage(bucketPage))) {
                                return rc;
                            }
                        }
                    }
                }

                // Else if the key is not present in the node
                else {
                    // If the node is not full
                    if (numberKeys < keyCapacity) {
                        // Find the position for this key
                        int position = numberKeys;
                        for (int i=0; i<numberKeys; i++) {
                            if (givenKey < keyArray[i]) {
                                position = i;
                                break;
                            }
                        }

                        // Move the other keys forward and insert the key
                        for (int i=numberKeys; i>position; i--) {
                            keyArray[i] = keyArray[i-1];
                            valueArray[i] = valueArray[i-1];
                        }
                        keyArray[position] = givenKey;
                        valueArray[position].state = RID_FILLED;
                        valueArray[position].rid = rid;
                        valueArray[position].page = IX_NO_PAGE;

                        nodeHeader->numberKeys++;

                        // Copy the keys and values to the node
                        memcpy(pageData, (char*) nodeHeader, sizeof(IX_NodeHeader));
                        memcpy(keyData, (char*) keyArray, attrLength*degree);
                        memcpy(valueData, (char*) valueArray, sizeof(IX_NodeValue)*(degree+1));
                    }

                    // If the node is full
                    else {
                        // Allocate a new node page
                        PF_PageHandle newPFPH;
                        char* newPageData;
                        PageNum newPageNumber;
                        if ((rc = pfFH.AllocatePage(newPFPH))) {
                            return rc;
                        }
                        if ((rc = newPFPH.GetData(newPageData))) {
                            return rc;
                        }
                        if ((rc = newPFPH.GetPageNum(newPageNumber))) {
                            return rc;
                        }
                        if ((rc = pfFH.MarkDirty(newPageNumber))) {
                            return rc;
                        }

                        // Copy half the keys to the new page
                        IX_NodeHeader* newNodeHeader = new IX_NodeHeader;
                        int* newKeyArray = new int[degree];
                        IX_NodeValue* newValueArray = new IX_NodeValue[degree+1];
                        for (int i=numberKeys/2; i<numberKeys; i++) {
                            newKeyArray[i-numberKeys/2] = keyArray[i];
                            newValueArray[i-numberKeys/2] = valueArray[i];
                        }

                        // Update the node headers
                        nodeHeader->numberKeys = numberKeys/2;
                        nodeHeader->type = LEAF;
                        newNodeHeader->numberKeys = numberKeys - numberKeys/2;
                        newNodeHeader->keyCapacity = keyCapacity;
                        newNodeHeader->type = LEAF;

                        // Update the last pointer in the left node
                        valueArray[keyCapacity].state = PAGE_ONLY;
                        valueArray[keyCapacity].page = newPageNumber;
                        valueArray[keyCapacity].rid = dummyRID;

                        // Insert the new key
                        if (givenKey < newKeyArray[0]) {
                            // Insert in the left node
                            int position = numberKeys/2;
                            for (int i=0; i<numberKeys/2; i++) {
                                if (givenKey < keyArray[i]) {
                                    position = i;
                                    break;
                                }
                            }
                            for (int i=numberKeys/2; i>position; i--) {
                                keyArray[i] = keyArray[i-1];
                                valueArray[i] = valueArray[i-1];
                            }
                            keyArray[position] = givenKey;
                            valueArray[position].state = RID_FILLED;
                            valueArray[position].rid = rid;
                            valueArray[position].page = IX_NO_PAGE;

                            nodeHeader->numberKeys++;
                        }
                        else {
                            // Insert in the right node
                            int position = numberKeys - numberKeys/2;
                            for (int i=0; i< numberKeys - numberKeys/2; i++) {
                                if (givenKey < newKeyArray[i]) {
                                    position = i;
                                    break;
                                }
                            }
                            for (int i=numberKeys-numberKeys/2; i>position; i--) {
                                newKeyArray[i] = newKeyArray[i-1];
                                newValueArray[i] = newValueArray[i-1];
                            }
                            newKeyArray[position] = givenKey;
                            newValueArray[position].state = RID_FILLED;
                            newValueArray[position].rid = rid;
                            newValueArray[position].page = IX_NO_PAGE;

                            newNodeHeader->numberKeys++;
                        }

                        // Allocate a new root page
                        PF_PageHandle newRootPFPH;
                        char* newRootPageData;
                        PageNum newRootPage;
                        if ((rc = pfFH.AllocatePage(newRootPFPH))) {
                            return rc;
                        }
                        if ((rc = newRootPFPH.GetData(newRootPageData))) {
                            return rc;
                        }
                        if ((rc = newRootPFPH.GetPageNum(newRootPage))) {
                            return rc;
                        }
                        if ((rc = pfFH.MarkDirty(newRootPage))) {
                            return rc;
                        }

                        // Set the keys and values
                        IX_NodeHeader* newRootNodeHeader = new IX_NodeHeader;
                        int* newRootKeyArray = new int[degree];
                        IX_NodeValue* newRootValueArray = new IX_NodeValue[degree+1];
                        newRootNodeHeader->numberKeys = 1;
                        newRootNodeHeader->keyCapacity = keyCapacity;
                        newRootNodeHeader->type = ROOT;
                        newRootNodeHeader->parent = IX_NO_PAGE;

                        newRootKeyArray[0] = newKeyArray[0];
                        newRootValueArray[0].state = PAGE_ONLY;
                        newRootValueArray[0].page = rootPage;
                        newRootValueArray[0].rid = dummyRID;
                        newRootValueArray[1].state = PAGE_ONLY;
                        newRootValueArray[1].page = newPageNumber;
                        newRootValueArray[1].rid = dummyRID;

                        // Copy the data to the root page
                        memcpy(newRootPageData, (char*) newRootNodeHeader, sizeof(IX_NodeHeader));
                        memcpy(newRootPageData+sizeof(IX_NodeHeader), (char*) newRootKeyArray, attrLength*degree);
                        memcpy(newRootPageData+sizeof(IX_NodeHeader)+attrLength*degree, (char*) newRootValueArray, sizeof(IX_NodeValue)*(degree+1));
                        delete newRootNodeHeader;
                        delete[] newRootKeyArray;
                        delete[] newRootValueArray;

                        // Update the parent pointers
                        nodeHeader->parent = newRootPage;
                        newNodeHeader->parent = newRootPage;

                        // Copy the data in the pages
                        memcpy(newPageData, (char*) newNodeHeader, sizeof(IX_NodeHeader));
                        memcpy(newPageData+sizeof(IX_NodeHeader), (char*) newKeyArray, attrLength*degree);
                        memcpy(newPageData+sizeof(IX_NodeHeader)+attrLength*degree, (char*) newValueArray, sizeof(IX_NodeValue)*(degree+1));
                        delete newNodeHeader;
                        delete[] newKeyArray;
                        delete[] newValueArray;

                        memcpy(pageData, (char*) nodeHeader, sizeof(IX_NodeHeader));
                        memcpy(keyData, (char*) keyArray, attrLength*degree);
                        memcpy(valueData, (char*) valueArray, sizeof(IX_NodeValue)*(degree+1));

                        // Update the root page in the index header
                        indexHeader.rootPage = newRootPage;
                        headerModified = TRUE;

                        // Unpin the new pages
                        if ((rc = pfFH.UnpinPage(newRootPage))) {
                            return rc;
                        }
                        if ((rc = pfFH.UnpinPage(newPageNumber))) {
                            return rc;
                        }
                    }
                }
            }
            else if (attrType == FLOAT) {
                // Check if pData is already a key
                float* keyArray = (float*) keyData;
                float givenKey = *static_cast<float*>(pData);
                int index = -1;
                for (int i=0; i<numberKeys; i++) {
                    if (keyArray[i] == givenKey) {
                        index = i;
                        break;
                    }
                }

                // If key exists, check if the same RID exists
                if (index != -1) {
                    IX_NodeValue value = valueArray[index];
                    if (compareRIDs(value.rid, rid)) {
                        return IX_ENTRY_EXISTS;
                    }
                    else {
                        // Get the bucket page
                        PageNum bucketPage = value.page;
                        if (bucketPage == IX_NO_PAGE) {
                            // Allocate a new bucket page
                            PF_PageHandle bucketPFPH;
                            if ((rc = pfFH.AllocatePage(bucketPFPH))) {
                                return rc;
                            }
                            if ((rc = bucketPFPH.GetPageNum(bucketPage))) {
                                return rc;
                            }
                            valueArray[index].page = bucketPage;
                            memcpy(valueData, (char*) valueArray, sizeof(IX_NodeValue)*(degree+1));

                            // Initialize and copy the bucket header
                            IX_BucketPageHeader* bucketHeader = new IX_BucketPageHeader;
                            bucketHeader->numberRecords = 1;
                            int recordCapacity = (PF_PAGE_SIZE-sizeof(IX_BucketPageHeader)) / (sizeof(RID));
                            bucketHeader->recordCapacity = recordCapacity;
                            // bucketHeader->nextBucket = IX_NO_PAGE;

                            char* bucketData;
                            if ((rc = bucketPFPH.GetData(bucketData))) {
                                return rc;
                            }
                            if ((rc = pfFH.MarkDirty(bucketPage))) {
                                return rc;
                            }
                            memcpy(bucketData, (char*) bucketHeader, sizeof(IX_BucketPageHeader));
                            delete bucketHeader;

                            // Copy the RID to the bucket
                            RID* ridList = new RID[recordCapacity];
                            ridList[0] = rid;
                            memcpy(bucketData+sizeof(IX_BucketPageHeader), ridList, sizeof(RID)*recordCapacity);

                            // Unpin the bucket page
                            if ((rc = pfFH.UnpinPage(bucketPage))) {
                                return rc;
                            }
                        }
                        else {
                            // Get the data from the bucket page
                            PF_PageHandle bucketPFPH;
                            char* bucketData;
                            if ((rc = pfFH.GetThisPage(bucketPage, bucketPFPH))) {
                                return rc;
                            }
                            if ((rc = bucketPFPH.GetData(bucketData))) {
                                return rc;
                            }
                            if ((rc = pfFH.MarkDirty(bucketPage))) {
                                return rc;
                            }

                            // Get the bucket page header
                            IX_BucketPageHeader* bucketHeader = (IX_BucketPageHeader*) bucketData;
                            int numberRecords = bucketHeader->numberRecords;
                            int recordCapacity = bucketHeader->recordCapacity;

                            // Check the entries in the bucket for the same rid
                            char* ridData = bucketData + sizeof(IX_BucketPageHeader);
                            RID* ridList = (RID*) ridData;
                            for (int i=0; i<numberRecords; i++) {
                                if (compareRIDs(ridList[i], rid)) {
                                    return IX_ENTRY_EXISTS;
                                }
                            }

                            // Insert the new RID if not found
                            if (numberRecords == recordCapacity) {
                                return IX_BUCKET_FULL;
                            }
                            ridList[numberRecords] = rid;
                            memcpy(ridData, (char*) ridList, sizeof(sizeof(RID)*recordCapacity));

                            bucketHeader->numberRecords++;
                            memcpy(bucketData, (char*) bucketHeader, sizeof(IX_BucketPageHeader));

                            // Unpin the bucket page
                            if ((rc = pfFH.UnpinPage(bucketPage))) {
                                return rc;
                            }
                        }
                    }
                }

                // Else if the key is not present in the node
                else {
                    // If the node is not full
                    if (numberKeys < keyCapacity) {
                        // Find the position for this key
                        int position = numberKeys;
                        for (int i=0; i<numberKeys; i++) {
                            if (givenKey < keyArray[i]) {
                                position = i;
                                break;
                            }
                        }

                        // Move the other keys forward and insert the key
                        for (int i=numberKeys; i>position; i--) {
                            keyArray[i] = keyArray[i-1];
                            valueArray[i] = valueArray[i-1];
                        }
                        keyArray[position] = givenKey;
                        valueArray[position].state = RID_FILLED;
                        valueArray[position].rid = rid;
                        valueArray[position].page = IX_NO_PAGE;

                        nodeHeader->numberKeys++;

                        // Copy the keys and values to the node
                        memcpy(keyData, (char*) keyArray, attrLength*degree);
                        memcpy(valueData, (char*) valueArray, sizeof(IX_NodeValue)*(degree+1));
                    }

                    // If the node is full
                    else {
                        // Allocate a new node page
                        PF_PageHandle newPFPH;
                        char* newPageData;
                        PageNum newPageNumber;
                        if ((rc = pfFH.AllocatePage(newPFPH))) {
                            return rc;
                        }
                        if ((rc = newPFPH.GetData(newPageData))) {
                            return rc;
                        }
                        if ((rc = newPFPH.GetPageNum(newPageNumber))) {
                            return rc;
                        }
                        if ((rc = pfFH.MarkDirty(newPageNumber))) {
                            return rc;
                        }

                        // Copy half the keys to the new page
                        IX_NodeHeader* newNodeHeader = new IX_NodeHeader;
                        float* newKeyArray = new float[degree];
                        IX_NodeValue* newValueArray = new IX_NodeValue[degree+1];
                        for (int i=numberKeys/2; i<numberKeys; i++) {
                            newKeyArray[i-numberKeys/2] = keyArray[i];
                            newValueArray[i-numberKeys/2] = valueArray[i];
                        }

                        // Update the node headers
                        nodeHeader->numberKeys = numberKeys/2;
                        nodeHeader->type = LEAF;
                        newNodeHeader->numberKeys = numberKeys - numberKeys/2;
                        newNodeHeader->keyCapacity = keyCapacity;
                        newNodeHeader->type = LEAF;

                        // Update the last pointer in the left node
                        valueArray[keyCapacity].state = PAGE_ONLY;
                        valueArray[keyCapacity].page = newPageNumber;
                        valueArray[keyCapacity].rid = dummyRID;

                        // Insert the new key
                        if (givenKey < newKeyArray[0]) {
                            // Insert in the left node
                            int position = numberKeys/2;
                            for (int i=0; i<numberKeys/2; i++) {
                                if (givenKey < keyArray[i]) {
                                    position = i;
                                    break;
                                }
                            }
                            for (int i=numberKeys/2; i>position; i--) {
                                keyArray[i] = keyArray[i-1];
                                valueArray[i] = valueArray[i-1];
                            }
                            keyArray[position] = givenKey;
                            valueArray[position].state = RID_FILLED;
                            valueArray[position].rid = rid;
                            valueArray[position].page = IX_NO_PAGE;

                            nodeHeader->numberKeys++;
                        }
                        else {
                            // Insert in the right node
                            int position = numberKeys - numberKeys/2;
                            for (int i=0; i< numberKeys - numberKeys/2; i++) {
                                if (givenKey < newKeyArray[i]) {
                                    position = i;
                                    break;
                                }
                            }
                            for (int i=numberKeys-numberKeys/2; i>position; i--) {
                                newKeyArray[i] = newKeyArray[i-1];
                                newValueArray[i] = newValueArray[i-1];
                            }
                            newKeyArray[position] = givenKey;
                            newValueArray[position].state = RID_FILLED;
                            newValueArray[position].rid = rid;
                            newValueArray[position].page = IX_NO_PAGE;

                            newNodeHeader->numberKeys++;
                        }

                        // Allocate a new root page
                        PF_PageHandle newRootPFPH;
                        char* newRootPageData;
                        PageNum newRootPage;
                        if ((rc = pfFH.AllocatePage(newRootPFPH))) {
                            return rc;
                        }
                        if ((rc = newRootPFPH.GetData(newRootPageData))) {
                            return rc;
                        }
                        if ((rc = newRootPFPH.GetPageNum(newRootPage))) {
                            return rc;
                        }
                        if ((rc = pfFH.MarkDirty(newRootPage))) {
                            return rc;
                        }

                        // Set the keys and values
                        IX_NodeHeader* newRootNodeHeader = new IX_NodeHeader;
                        float* newRootKeyArray = new float[degree];
                        IX_NodeValue* newRootValueArray = new IX_NodeValue[degree+1];
                        newRootNodeHeader->numberKeys = 1;
                        newRootNodeHeader->keyCapacity = keyCapacity;
                        newRootNodeHeader->type = ROOT;
                        newRootNodeHeader->parent = IX_NO_PAGE;

                        newRootKeyArray[0] = newKeyArray[0];
                        newRootValueArray[0].state = PAGE_ONLY;
                        newRootValueArray[0].page = rootPage;
                        newRootValueArray[0].rid = dummyRID;
                        newRootValueArray[1].state = PAGE_ONLY;
                        newRootValueArray[1].page = newPageNumber;
                        newRootValueArray[1].rid = dummyRID;

                        // Copy the data to the root page
                        memcpy(newRootPageData, (char*) newRootNodeHeader, sizeof(IX_NodeHeader));
                        memcpy(newRootPageData+sizeof(IX_NodeHeader), (char*) newRootKeyArray, attrLength*degree);
                        memcpy(newRootPageData+sizeof(IX_NodeHeader)+attrLength*degree, (char*) newRootValueArray, sizeof(IX_NodeValue)*(degree+1));
                        delete newRootNodeHeader;
                        delete[] newRootKeyArray;
                        delete[] newRootValueArray;

                        // Update the parent pointers
                        nodeHeader->parent = newRootPage;
                        newNodeHeader->parent = newRootPage;

                        // Copy the data in the pages
                        memcpy(newPageData, (char*) newNodeHeader, sizeof(IX_NodeHeader));
                        memcpy(newPageData+sizeof(IX_NodeHeader), (char*) newKeyArray, attrLength*degree);
                        memcpy(newPageData+sizeof(IX_NodeHeader)+attrLength*degree, (char*) newValueArray, sizeof(IX_NodeValue)*(degree+1));
                        delete newNodeHeader;
                        delete[] newKeyArray;
                        delete[] newValueArray;

                        memcpy(pageData, (char*) nodeHeader, sizeof(IX_NodeHeader));
                        memcpy(keyData, (char*) keyArray, attrLength*degree);
                        memcpy(valueData, (char*) valueArray, sizeof(IX_NodeValue)*(degree+1));

                        // Update the root page in the index header
                        indexHeader.rootPage = newRootPage;
                        headerModified = TRUE;

                        // Unpin the new pages
                        if ((rc = pfFH.UnpinPage(newRootPage))) {
                            return rc;
                        }
                        if ((rc = pfFH.UnpinPage(newPageNumber))) {
                            return rc;
                        }
                    }
                }
            }
            else {
                // Check if pData is already a key
                string* keyArray = (string*) keyData;
                char* givenKeyChar = static_cast<char*>(pData);
                string givenKey = "";
                for (int i=0; i < attrLength; i++) {
                    givenKey += givenKeyChar[i];
                }
                int index = -1;
                for (int i=0; i<numberKeys; i++) {
                    if (keyArray[i] == givenKey) {
                        index = i;
                        break;
                    }
                }

                // If key exists, check if the same RID exists
                if (index != -1) {
                    IX_NodeValue value = valueArray[index];
                    if (compareRIDs(value.rid, rid)) {
                        return IX_ENTRY_EXISTS;
                    }
                    else {
                        // Get the bucket page
                        PageNum bucketPage = value.page;
                        if (bucketPage == IX_NO_PAGE) {
                            // Allocate a new bucket page
                            PF_PageHandle bucketPFPH;
                            if ((rc = pfFH.AllocatePage(bucketPFPH))) {
                                return rc;
                            }
                            if ((rc = bucketPFPH.GetPageNum(bucketPage))) {
                                return rc;
                            }
                            valueArray[index].page = bucketPage;
                            memcpy(valueData, (char*) valueArray, sizeof(IX_NodeValue)*(degree+1));

                            // Initialize and copy the bucket header
                            IX_BucketPageHeader* bucketHeader = new IX_BucketPageHeader;
                            bucketHeader->numberRecords = 1;
                            int recordCapacity = (PF_PAGE_SIZE-sizeof(IX_BucketPageHeader)) / (sizeof(RID));
                            bucketHeader->recordCapacity = recordCapacity;
                            // bucketHeader->nextBucket = IX_NO_PAGE;

                            char* bucketData;
                            if ((rc = bucketPFPH.GetData(bucketData))) {
                                return rc;
                            }
                            if ((rc = pfFH.MarkDirty(bucketPage))) {
                                return rc;
                            }
                            memcpy(bucketData, (char*) bucketHeader, sizeof(IX_BucketPageHeader));
                            delete bucketHeader;

                            // Copy the RID to the bucket
                            RID* ridList = new RID[recordCapacity];
                            ridList[0] = rid;
                            memcpy(bucketData+sizeof(IX_BucketPageHeader), ridList, sizeof(RID)*recordCapacity);

                            // Unpin the bucket page
                            if ((rc = pfFH.UnpinPage(bucketPage))) {
                                return rc;
                            }
                        }
                        else {
                            // Get the data from the bucket page
                            PF_PageHandle bucketPFPH;
                            char* bucketData;
                            if ((rc = pfFH.GetThisPage(bucketPage, bucketPFPH))) {
                                return rc;
                            }
                            if ((rc = bucketPFPH.GetData(bucketData))) {
                                return rc;
                            }
                            if ((rc = pfFH.MarkDirty(bucketPage))) {
                                return rc;
                            }

                            // Get the bucket page header
                            IX_BucketPageHeader* bucketHeader = (IX_BucketPageHeader*) bucketData;
                            int numberRecords = bucketHeader->numberRecords;
                            int recordCapacity = bucketHeader->recordCapacity;

                            // Check the entries in the bucket for the same rid
                            char* ridData = bucketData + sizeof(IX_BucketPageHeader);
                            RID* ridList = (RID*) ridData;
                            for (int i=0; i<numberRecords; i++) {
                                if (compareRIDs(ridList[i], rid)) {
                                    return IX_ENTRY_EXISTS;
                                }
                            }

                            // Insert the new RID if not found
                            if (numberRecords == recordCapacity) {
                                return IX_BUCKET_FULL;
                            }
                            ridList[numberRecords] = rid;
                            memcpy(ridData, (char*) ridList, sizeof(sizeof(RID)*recordCapacity));

                            bucketHeader->numberRecords++;
                            memcpy(bucketData, (char*) bucketHeader, sizeof(IX_BucketPageHeader));

                            // Unpin the bucket page
                            if ((rc = pfFH.UnpinPage(bucketPage))) {
                                return rc;
                            }
                        }
                    }
                }

                // Else if the key is not present in the node
                else {
                    // If the node is not full
                    if (numberKeys < keyCapacity) {
                        // Find the position for this key
                        int position = numberKeys;
                        for (int i=0; i<numberKeys; i++) {
                            if (givenKey < keyArray[i]) {
                                position = i;
                                break;
                            }
                        }

                        // Move the other keys forward and insert the key
                        for (int i=numberKeys; i>position; i--) {
                            keyArray[i] = keyArray[i-1];
                            valueArray[i] = valueArray[i-1];
                        }
                        keyArray[position] = givenKey;
                        valueArray[position].state = RID_FILLED;
                        valueArray[position].rid = rid;
                        valueArray[position].page = IX_NO_PAGE;

                        nodeHeader->numberKeys++;

                        // Copy the keys and values to the node
                        memcpy(keyData, (char*) keyArray, attrLength*degree);
                        memcpy(valueData, (char*) valueArray, sizeof(IX_NodeValue)*(degree+1));
                    }

                    // If the node is full
                    else {
                        // Allocate a new node page
                        PF_PageHandle newPFPH;
                        char* newPageData;
                        PageNum newPageNumber;
                        if ((rc = pfFH.AllocatePage(newPFPH))) {
                            return rc;
                        }
                        if ((rc = newPFPH.GetData(newPageData))) {
                            return rc;
                        }
                        if ((rc = newPFPH.GetPageNum(newPageNumber))) {
                            return rc;
                        }
                        if ((rc = pfFH.MarkDirty(newPageNumber))) {
                            return rc;
                        }

                        // Copy half the keys to the new page
                        IX_NodeHeader* newNodeHeader = new IX_NodeHeader;
                        string* newKeyArray = new string[degree];
                        IX_NodeValue* newValueArray = new IX_NodeValue[degree+1];
                        for (int i=numberKeys/2; i<numberKeys; i++) {
                            newKeyArray[i-numberKeys/2] = keyArray[i];
                            newValueArray[i-numberKeys/2] = valueArray[i];
                        }

                        // Update the node headers
                        nodeHeader->numberKeys = numberKeys/2;
                        nodeHeader->type = LEAF;
                        newNodeHeader->numberKeys = numberKeys - numberKeys/2;
                        newNodeHeader->keyCapacity = keyCapacity;
                        newNodeHeader->type = LEAF;

                        // Update the last pointer in the left node
                        valueArray[keyCapacity].state = PAGE_ONLY;
                        valueArray[keyCapacity].page = newPageNumber;
                        valueArray[keyCapacity].rid = dummyRID;

                        // Insert the new key
                        if (givenKey < newKeyArray[0]) {
                            // Insert in the left node
                            int position = numberKeys/2;
                            for (int i=0; i<numberKeys/2; i++) {
                                if (givenKey < keyArray[i]) {
                                    position = i;
                                    break;
                                }
                            }
                            for (int i=numberKeys/2; i>position; i--) {
                                keyArray[i] = keyArray[i-1];
                                valueArray[i] = valueArray[i-1];
                            }
                            keyArray[position] = givenKey;
                            valueArray[position].state = RID_FILLED;
                            valueArray[position].rid = rid;
                            valueArray[position].page = IX_NO_PAGE;

                            nodeHeader->numberKeys++;
                        }
                        else {
                            // Insert in the right node
                            int position = numberKeys - numberKeys/2;
                            for (int i=0; i< numberKeys - numberKeys/2; i++) {
                                if (givenKey < newKeyArray[i]) {
                                    position = i;
                                    break;
                                }
                            }
                            for (int i=numberKeys-numberKeys/2; i>position; i--) {
                                newKeyArray[i] = newKeyArray[i-1];
                                newValueArray[i] = newValueArray[i-1];
                            }
                            newKeyArray[position] = givenKey;
                            newValueArray[position].state = RID_FILLED;
                            newValueArray[position].rid = rid;
                            newValueArray[position].page = IX_NO_PAGE;

                            newNodeHeader->numberKeys++;
                        }

                        // Allocate a new root page
                        PF_PageHandle newRootPFPH;
                        char* newRootPageData;
                        PageNum newRootPage;
                        if ((rc = pfFH.AllocatePage(newRootPFPH))) {
                            return rc;
                        }
                        if ((rc = newRootPFPH.GetData(newRootPageData))) {
                            return rc;
                        }
                        if ((rc = newRootPFPH.GetPageNum(newRootPage))) {
                            return rc;
                        }
                        if ((rc = pfFH.MarkDirty(newRootPage))) {
                            return rc;
                        }

                        // Set the keys and values
                        IX_NodeHeader* newRootNodeHeader = new IX_NodeHeader;
                        string* newRootKeyArray = new string[degree];
                        IX_NodeValue* newRootValueArray = new IX_NodeValue[degree+1];
                        newRootNodeHeader->numberKeys = 1;
                        newRootNodeHeader->keyCapacity = keyCapacity;
                        newRootNodeHeader->type = ROOT;
                        newRootNodeHeader->parent = IX_NO_PAGE;

                        newRootKeyArray[0] = newKeyArray[0];
                        newRootValueArray[0].state = PAGE_ONLY;
                        newRootValueArray[0].page = rootPage;
                        newRootValueArray[0].rid = dummyRID;
                        newRootValueArray[1].state = PAGE_ONLY;
                        newRootValueArray[1].page = newPageNumber;
                        newRootValueArray[1].rid = dummyRID;

                        // Copy the data to the root page
                        memcpy(newRootPageData, (char*) newRootNodeHeader, sizeof(IX_NodeHeader));
                        memcpy(newRootPageData+sizeof(IX_NodeHeader), (char*) newRootKeyArray, attrLength*degree);
                        memcpy(newRootPageData+sizeof(IX_NodeHeader)+attrLength*degree, (char*) newRootValueArray, sizeof(IX_NodeValue)*(degree+1));
                        delete newRootNodeHeader;
                        delete[] newRootKeyArray;
                        delete[] newRootValueArray;

                        // Update the parent pointers
                        nodeHeader->parent = newRootPage;
                        newNodeHeader->parent = newRootPage;

                        // Copy the data in the pages
                        memcpy(newPageData, (char*) newNodeHeader, sizeof(IX_NodeHeader));
                        memcpy(newPageData+sizeof(IX_NodeHeader), (char*) newKeyArray, attrLength*degree);
                        memcpy(newPageData+sizeof(IX_NodeHeader)+attrLength*degree, (char*) newValueArray, sizeof(IX_NodeValue)*(degree+1));
                        delete newNodeHeader;
                        delete[] newKeyArray;
                        delete[] newValueArray;

                        memcpy(pageData, (char*) nodeHeader, sizeof(IX_NodeHeader));
                        memcpy(keyData, (char*) keyArray, attrLength*degree);
                        memcpy(valueData, (char*) valueArray, sizeof(IX_NodeValue)*(degree+1));

                        // Update the root page in the index header
                        indexHeader.rootPage = newRootPage;
                        headerModified = TRUE;

                        // Unpin the new pages
                        if ((rc = pfFH.UnpinPage(newRootPage))) {
                            return rc;
                        }
                        if ((rc = pfFH.UnpinPage(newPageNumber))) {
                            return rc;
                        }
                    }
                }
            }
        }

        // Else if the type is ROOT
        else {
            // Call the recursive function
            if ((rc = InsertEntryRecursive(pData, rid, rootPage))) {
                return rc;
            }
        }

        // Unpin the root page
        if ((rc = pfFH.UnpinPage(rootPage))) {
            return rc;
        }

        // Return OK
        return OK_RC;
    }
}

// Method: InsertEntryRecursive(void *pData, const RID &rid, PageNum node)
// Insert a new index entry
/* Steps:
    1) Get the data in the node
    2) If node is a leaf
        - If pData exists in the node, check if RID is same
            - If not same, go to bucket page (allocate if not present)
            - Insert RID in the bucket page
        - Else if pData does not exist
            - If number of keys < capacity, insert new entry in the node
            - If number of keys = capacity
                - Split the node into 2 nodes (allocate new page)
                - Set type of new node to LEAF
                - Call the recursive pushKeyUp function
                - Unpin all pages
    3) Else if the node is root/internal node
        - Search for corresponding pointer to next node
        - Recursive call with the next node page number
        - Unpin the current node page
*/
RC IX_IndexHandle::InsertEntryRecursive(void *pData, const RID &rid, PageNum node) {
    // Declare an integer for the return code
    int rc;

    // Get the index header information
    int degree = indexHeader.degree;
    int attrLength = indexHeader.attrLength;
    AttrType attrType = indexHeader.attrType;

    // Get the data in the node
    PF_PageHandle pfPH;
    char* nodeData;
    if ((rc = pfFH.GetThisPage(node, pfPH))) {
        return rc;
    }
    if ((rc = pfPH.GetData(nodeData))) {
        return rc;
    }
    if ((rc = pfFH.MarkDirty(node))) {
        return rc;
    }

    // Check the node header information
    IX_NodeHeader* nodeHeader = (IX_NodeHeader*) nodeData;
    IX_NodeType type = nodeHeader->type;
    int numberKeys = nodeHeader->numberKeys;
    int keyCapacity = nodeHeader->keyCapacity;
    char* keyData = nodeData + sizeof(IX_NodeHeader);
    char* valueData = keyData + attrLength*degree;
    IX_NodeValue* valueArray = (IX_NodeValue*) valueData;

    // If the node is a LEAF
    if (type == LEAF) {
        if (attrType ==  INT) {
            // Check if pData is already a key
            int* keyArray = (int*) keyData;
            int givenKey = *static_cast<int*>(pData);
            int index = -1;
            for (int i=0; i<numberKeys; i++) {
                if (keyArray[i] == givenKey) {
                    index = i;
                    break;
                }
            }

            // If key exists, check if the same RID exists
            if (index != -1) {
                IX_NodeValue value = valueArray[index];
                if (compareRIDs(value.rid, rid)) {
                    return IX_ENTRY_EXISTS;
                }
                else {
                    // Get the bucket page
                    PageNum bucketPage = value.page;
                    if (bucketPage == IX_NO_PAGE) {
                        // Allocate a new bucket page
                        PF_PageHandle bucketPFPH;
                        if ((rc = pfFH.AllocatePage(bucketPFPH))) {
                            return rc;
                        }
                        if ((rc = bucketPFPH.GetPageNum(bucketPage))) {
                            return rc;
                        }
                        valueArray[index].page = bucketPage;
                        memcpy(valueData, (char*) valueArray, sizeof(IX_NodeValue)*(degree+1));

                        // Initialize and copy the bucket header
                        IX_BucketPageHeader* bucketHeader = new IX_BucketPageHeader;
                        bucketHeader->numberRecords = 1;
                        int recordCapacity = (PF_PAGE_SIZE-sizeof(IX_BucketPageHeader)) / (sizeof(RID));
                        bucketHeader->recordCapacity = recordCapacity;
                        // bucketHeader->nextBucket = IX_NO_PAGE;

                        char* bucketData;
                        if ((rc = bucketPFPH.GetData(bucketData))) {
                            return rc;
                        }
                        if ((rc = pfFH.MarkDirty(bucketPage))) {
                            return rc;
                        }
                        memcpy(bucketData, (char*) bucketHeader, sizeof(IX_BucketPageHeader));
                        delete bucketHeader;

                        // Copy the RID to the bucket
                        RID* ridList = new RID[recordCapacity];
                        ridList[0] = rid;
                        memcpy(bucketData+sizeof(IX_BucketPageHeader), ridList, sizeof(RID)*recordCapacity);

                        // Unpin the bucket page
                        if ((rc = pfFH.UnpinPage(bucketPage))) {
                            return rc;
                        }
                    }
                    else {
                        // Get the data from the bucket page
                        PF_PageHandle bucketPFPH;
                        char* bucketData;
                        if ((rc = pfFH.GetThisPage(bucketPage, bucketPFPH))) {
                            return rc;
                        }
                        if ((rc = bucketPFPH.GetData(bucketData))) {
                            return rc;
                        }
                        if ((rc = pfFH.MarkDirty(bucketPage))) {
                            return rc;
                        }

                        // Get the bucket page header
                        IX_BucketPageHeader* bucketHeader = (IX_BucketPageHeader*) bucketData;
                        int numberRecords = bucketHeader->numberRecords;
                        int recordCapacity = bucketHeader->recordCapacity;

                        // Check the entries in the bucket for the same rid
                        char* ridData = bucketData + sizeof(IX_BucketPageHeader);
                        RID* ridList = (RID*) ridData;
                        for (int i=0; i<numberRecords; i++) {
                            if (compareRIDs(ridList[i], rid)) {
                                return IX_ENTRY_EXISTS;
                            }
                        }

                        // Insert the new RID if not found
                        if (numberRecords == recordCapacity) {
                            return IX_BUCKET_FULL;
                        }
                        ridList[numberRecords] = rid;
                        memcpy(ridData, (char*) ridList, sizeof(sizeof(RID)*recordCapacity));

                        bucketHeader->numberRecords++;
                        memcpy(bucketData, (char*) bucketHeader, sizeof(IX_BucketPageHeader));

                        // Unpin the bucket page
                        if ((rc = pfFH.UnpinPage(bucketPage))) {
                            return rc;
                        }
                    }
                }
            }

            // Else if the key is not present in the node
            else {
                // If the node is not full
                if (numberKeys < keyCapacity) {
                    // Find the position for this key
                    int position = numberKeys;
                    for (int i=0; i<numberKeys; i++) {
                        if (givenKey < keyArray[i]) {
                            position = i;
                            break;
                        }
                    }

                    // Move the other keys forward and insert the key
                    for (int i=numberKeys; i>position; i--) {
                        keyArray[i] = keyArray[i-1];
                        valueArray[i] = valueArray[i-1];
                    }
                    keyArray[position] = givenKey;
                    valueArray[position].state = RID_FILLED;
                    valueArray[position].rid = rid;
                    valueArray[position].page = IX_NO_PAGE;

                    // Copy the keys and values to the node
                    memcpy(keyData, (char*) keyArray, attrLength*degree);
                    memcpy(valueData, (char*) valueArray, sizeof(IX_NodeValue)*(degree+1));
                }

                // If the node is full
                else {
                    // Allocate a new node page
                    PF_PageHandle newPFPH;
                    char* newPageData;
                    PageNum newPageNumber;
                    if ((rc = pfFH.AllocatePage(newPFPH))) {
                        return rc;
                    }
                    if ((rc = newPFPH.GetData(newPageData))) {
                        return rc;
                    }
                    if ((rc = newPFPH.GetPageNum(newPageNumber))) {
                        return rc;
                    }
                    if ((rc = pfFH.MarkDirty(newPageNumber))) {
                        return rc;
                    }

                    // Copy half the keys to the new page
                    IX_NodeHeader* newNodeHeader = new IX_NodeHeader;
                    int* newKeyArray = new int[degree];
                    IX_NodeValue* newValueArray = new IX_NodeValue[degree+1];
                    for (int i=numberKeys/2; i<numberKeys; i++) {
                        newKeyArray[i-numberKeys/2] = keyArray[i];
                        newValueArray[i-numberKeys/2] = valueArray[i];
                    }

                    // Update the node headers
                    nodeHeader->numberKeys = numberKeys/2;
                    nodeHeader->type = LEAF;
                    newNodeHeader->numberKeys = numberKeys - numberKeys/2;
                    newNodeHeader->keyCapacity = keyCapacity;
                    newNodeHeader->type = LEAF;

                    // Update the last pointer in the left node
                    valueArray[keyCapacity].state = PAGE_ONLY;
                    valueArray[keyCapacity].page = newPageNumber;
                    valueArray[keyCapacity].rid = dummyRID;

                    // Insert the new key
                    if (givenKey < newKeyArray[0]) {
                        // Insert in the left node
                        int position = numberKeys/2;
                        for (int i=0; i<numberKeys/2; i++) {
                            if (givenKey < keyArray[i]) {
                                position = i;
                                break;
                            }
                        }
                        for (int i=numberKeys/2; i>position; i--) {
                            keyArray[i] = keyArray[i-1];
                            valueArray[i] = valueArray[i-1];
                        }
                        keyArray[position] = givenKey;
                        valueArray[position].state = RID_FILLED;
                        valueArray[position].rid = rid;
                        valueArray[position].page = IX_NO_PAGE;

                        nodeHeader->numberKeys++;
                    }
                    else {
                        // Insert in the right node
                        int position = numberKeys - numberKeys/2;
                        for (int i=0; i< numberKeys - numberKeys/2; i++) {
                            if (givenKey < newKeyArray[i]) {
                                position = i;
                                break;
                            }
                        }
                        for (int i=numberKeys-numberKeys/2; i>position; i--) {
                            newKeyArray[i] = newKeyArray[i-1];
                            newValueArray[i] = newValueArray[i-1];
                        }
                        newKeyArray[position] = givenKey;
                        newValueArray[position].state = RID_FILLED;
                        newValueArray[position].rid = rid;
                        newValueArray[position].page = IX_NO_PAGE;

                        newNodeHeader->numberKeys++;
                    }

                    // Get the parent node
                    PageNum parentNode = nodeHeader->parent;
                    newNodeHeader->parent = parentNode;

                    // Call the pushKeyUp function to push the new key up
                    if ((rc = pushKeyUp((void*) newKeyArray, parentNode, node, newPageNumber))) {
                        return rc;
                    }

                    // Unpin the new page
                    if ((rc = pfFH.UnpinPage(newPageNumber))) {
                        return rc;
                    }
                }
            }
        }
        else if (attrType == FLOAT) {
            // Check if pData is already a key
            float* keyArray = (float*) keyData;
            float givenKey = *static_cast<float*>(pData);
            int index = -1;
            for (int i=0; i<numberKeys; i++) {
                if (keyArray[i] == givenKey) {
                    index = i;
                    break;
                }
            }

            // If key exists, check if the same RID exists
            if (index != -1) {
                IX_NodeValue value = valueArray[index];
                if (compareRIDs(value.rid, rid)) {
                    return IX_ENTRY_EXISTS;
                }
                else {
                    // Get the bucket page
                    PageNum bucketPage = value.page;
                    if (bucketPage == IX_NO_PAGE) {
                        // Allocate a new bucket page
                        PF_PageHandle bucketPFPH;
                        if ((rc = pfFH.AllocatePage(bucketPFPH))) {
                            return rc;
                        }
                        if ((rc = bucketPFPH.GetPageNum(bucketPage))) {
                            return rc;
                        }
                        valueArray[index].page = bucketPage;
                        memcpy(valueData, (char*) valueArray, sizeof(IX_NodeValue)*(degree+1));

                        // Initialize and copy the bucket header
                        IX_BucketPageHeader* bucketHeader = new IX_BucketPageHeader;
                        bucketHeader->numberRecords = 1;
                        int recordCapacity = (PF_PAGE_SIZE-sizeof(IX_BucketPageHeader)) / (sizeof(RID));
                        bucketHeader->recordCapacity = recordCapacity;
                        // bucketHeader->nextBucket = IX_NO_PAGE;

                        char* bucketData;
                        if ((rc = bucketPFPH.GetData(bucketData))) {
                            return rc;
                        }
                        if ((rc = pfFH.MarkDirty(bucketPage))) {
                            return rc;
                        }
                        memcpy(bucketData, (char*) bucketHeader, sizeof(IX_BucketPageHeader));
                        delete bucketHeader;

                        // Copy the RID to the bucket
                        RID* ridList = new RID[recordCapacity];
                        ridList[0] = rid;
                        memcpy(bucketData+sizeof(IX_BucketPageHeader), ridList, sizeof(RID)*recordCapacity);

                        // Unpin the bucket page
                        if ((rc = pfFH.UnpinPage(bucketPage))) {
                            return rc;
                        }
                    }
                    else {
                        // Get the data from the bucket page
                        PF_PageHandle bucketPFPH;
                        char* bucketData;
                        if ((rc = pfFH.GetThisPage(bucketPage, bucketPFPH))) {
                            return rc;
                        }
                        if ((rc = bucketPFPH.GetData(bucketData))) {
                            return rc;
                        }
                        if ((rc = pfFH.MarkDirty(bucketPage))) {
                            return rc;
                        }

                        // Get the bucket page header
                        IX_BucketPageHeader* bucketHeader = (IX_BucketPageHeader*) bucketData;
                        int numberRecords = bucketHeader->numberRecords;
                        int recordCapacity = bucketHeader->recordCapacity;

                        // Check the entries in the bucket for the same rid
                        char* ridData = bucketData + sizeof(IX_BucketPageHeader);
                        RID* ridList = (RID*) ridData;
                        for (int i=0; i<numberRecords; i++) {
                            if (compareRIDs(ridList[i], rid)) {
                                return IX_ENTRY_EXISTS;
                            }
                        }

                        // Insert the new RID if not found
                        if (numberRecords == recordCapacity) {
                            return IX_BUCKET_FULL;
                        }
                        ridList[numberRecords] = rid;
                        memcpy(ridData, (char*) ridList, sizeof(sizeof(RID)*recordCapacity));

                        bucketHeader->numberRecords++;
                        memcpy(bucketData, (char*) bucketHeader, sizeof(IX_BucketPageHeader));

                        // Unpin the bucket page
                        if ((rc = pfFH.UnpinPage(bucketPage))) {
                            return rc;
                        }
                    }
                }
            }

            // Else if the key is not present in the node
            else {
                // If the node is not full
                if (numberKeys < keyCapacity) {
                    // Find the position for this key
                    int position = numberKeys;
                    for (int i=0; i<numberKeys; i++) {
                        if (givenKey < keyArray[i]) {
                            position = i;
                            break;
                        }
                    }

                    // Move the other keys forward and insert the key
                    for (int i=numberKeys; i>position; i--) {
                        keyArray[i] = keyArray[i-1];
                        valueArray[i] = valueArray[i-1];
                    }
                    keyArray[position] = givenKey;
                    valueArray[position].state = RID_FILLED;
                    valueArray[position].rid = rid;
                    valueArray[position].page = IX_NO_PAGE;

                    // Copy the keys and values to the node
                    memcpy(keyData, (char*) keyArray, attrLength*degree);
                    memcpy(valueData, (char*) valueArray, sizeof(IX_NodeValue)*(degree+1));
                }

                // If the node is full
                else {
                    // Allocate a new node page
                    PF_PageHandle newPFPH;
                    char* newPageData;
                    PageNum newPageNumber;
                    if ((rc = pfFH.AllocatePage(newPFPH))) {
                        return rc;
                    }
                    if ((rc = newPFPH.GetData(newPageData))) {
                        return rc;
                    }
                    if ((rc = newPFPH.GetPageNum(newPageNumber))) {
                        return rc;
                    }
                    if ((rc = pfFH.MarkDirty(newPageNumber))) {
                        return rc;
                    }

                    // Copy half the keys to the new page
                    IX_NodeHeader* newNodeHeader = new IX_NodeHeader;
                    float* newKeyArray = new float[degree];
                    IX_NodeValue* newValueArray = new IX_NodeValue[degree+1];
                    for (int i=numberKeys/2; i<numberKeys; i++) {
                        newKeyArray[i-numberKeys/2] = keyArray[i];
                        newValueArray[i-numberKeys/2] = valueArray[i];
                    }

                    // Update the node headers
                    nodeHeader->numberKeys = numberKeys/2;
                    nodeHeader->type = LEAF;
                    newNodeHeader->numberKeys = numberKeys - numberKeys/2;
                    newNodeHeader->keyCapacity = keyCapacity;
                    newNodeHeader->type = LEAF;

                    // Update the last pointer in the left node
                    valueArray[keyCapacity].state = PAGE_ONLY;
                    valueArray[keyCapacity].page = newPageNumber;
                    valueArray[keyCapacity].rid = dummyRID;

                    // Insert the new key
                    if (givenKey < newKeyArray[0]) {
                        // Insert in the left node
                        int position = numberKeys/2;
                        for (int i=0; i<numberKeys/2; i++) {
                            if (givenKey < keyArray[i]) {
                                position = i;
                                break;
                            }
                        }
                        for (int i=numberKeys/2; i>position; i--) {
                            keyArray[i] = keyArray[i-1];
                            valueArray[i] = valueArray[i-1];
                        }
                        keyArray[position] = givenKey;
                        valueArray[position].state = RID_FILLED;
                        valueArray[position].rid = rid;
                        valueArray[position].page = IX_NO_PAGE;

                        nodeHeader->numberKeys++;
                    }
                    else {
                        // Insert in the right node
                        int position = numberKeys - numberKeys/2;
                        for (int i=0; i< numberKeys - numberKeys/2; i++) {
                            if (givenKey < newKeyArray[i]) {
                                position = i;
                                break;
                            }
                        }
                        for (int i=numberKeys-numberKeys/2; i>position; i--) {
                            newKeyArray[i] = newKeyArray[i-1];
                            newValueArray[i] = newValueArray[i-1];
                        }
                        newKeyArray[position] = givenKey;
                        newValueArray[position].state = RID_FILLED;
                        newValueArray[position].rid = rid;
                        newValueArray[position].page = IX_NO_PAGE;

                        newNodeHeader->numberKeys++;
                    }

                    // Get the parent node
                    PageNum parentNode = nodeHeader->parent;
                    newNodeHeader->parent = parentNode;

                    // Call the pushKeyUp function to push the new key up
                    if ((rc = pushKeyUp((void*) newKeyArray, parentNode, node, newPageNumber))) {
                        return rc;
                    }

                    // Unpin the new page
                    if ((rc = pfFH.UnpinPage(newPageNumber))) {
                        return rc;
                    }
                }
            }
        }
        else {
            // Check if pData is already a key
            string* keyArray = (string*) keyData;
            char* givenKeyChar = static_cast<char*>(pData);
            string givenKey = "";
            for (int i=0; i < indexHeader.attrLength; i++) {
                givenKey += givenKeyChar[i];
            }
            int index = -1;
            for (int i=0; i<numberKeys; i++) {
                if (keyArray[i] == givenKey) {
                    index = i;
                    break;
                }
            }

            // If key exists, check if the same RID exists
            if (index != -1) {
                IX_NodeValue value = valueArray[index];
                if (compareRIDs(value.rid, rid)) {
                    return IX_ENTRY_EXISTS;
                }
                else {
                    // Get the bucket page
                    PageNum bucketPage = value.page;
                    if (bucketPage == IX_NO_PAGE) {
                        // Allocate a new bucket page
                        PF_PageHandle bucketPFPH;
                        if ((rc = pfFH.AllocatePage(bucketPFPH))) {
                            return rc;
                        }
                        if ((rc = bucketPFPH.GetPageNum(bucketPage))) {
                            return rc;
                        }
                        valueArray[index].page = bucketPage;
                        memcpy(valueData, (char*) valueArray, sizeof(IX_NodeValue)*(degree+1));

                        // Initialize and copy the bucket header
                        IX_BucketPageHeader* bucketHeader = new IX_BucketPageHeader;
                        bucketHeader->numberRecords = 1;
                        int recordCapacity = (PF_PAGE_SIZE-sizeof(IX_BucketPageHeader)) / (sizeof(RID));
                        bucketHeader->recordCapacity = recordCapacity;
                        // bucketHeader->nextBucket = IX_NO_PAGE;

                        char* bucketData;
                        if ((rc = bucketPFPH.GetData(bucketData))) {
                            return rc;
                        }
                        if ((rc = pfFH.MarkDirty(bucketPage))) {
                            return rc;
                        }
                        memcpy(bucketData, (char*) bucketHeader, sizeof(IX_BucketPageHeader));
                        delete bucketHeader;

                        // Copy the RID to the bucket
                        RID* ridList = new RID[recordCapacity];
                        ridList[0] = rid;
                        memcpy(bucketData+sizeof(IX_BucketPageHeader), ridList, sizeof(RID)*recordCapacity);

                        // Unpin the bucket page
                        if ((rc = pfFH.UnpinPage(bucketPage))) {
                            return rc;
                        }
                    }
                    else {
                        // Get the data from the bucket page
                        PF_PageHandle bucketPFPH;
                        char* bucketData;
                        if ((rc = pfFH.GetThisPage(bucketPage, bucketPFPH))) {
                            return rc;
                        }
                        if ((rc = bucketPFPH.GetData(bucketData))) {
                            return rc;
                        }
                        if ((rc = pfFH.MarkDirty(bucketPage))) {
                            return rc;
                        }

                        // Get the bucket page header
                        IX_BucketPageHeader* bucketHeader = (IX_BucketPageHeader*) bucketData;
                        int numberRecords = bucketHeader->numberRecords;
                        int recordCapacity = bucketHeader->recordCapacity;

                        // Check the entries in the bucket for the same rid
                        char* ridData = bucketData + sizeof(IX_BucketPageHeader);
                        RID* ridList = (RID*) ridData;
                        for (int i=0; i<numberRecords; i++) {
                            if (compareRIDs(ridList[i], rid)) {
                                return IX_ENTRY_EXISTS;
                            }
                        }

                        // Insert the new RID if not found
                        if (numberRecords == recordCapacity) {
                            return IX_BUCKET_FULL;
                        }
                        ridList[numberRecords] = rid;
                        memcpy(ridData, (char*) ridList, sizeof(sizeof(RID)*recordCapacity));

                        bucketHeader->numberRecords++;
                        memcpy(bucketData, (char*) bucketHeader, sizeof(IX_BucketPageHeader));

                        // Unpin the bucket page
                        if ((rc = pfFH.UnpinPage(bucketPage))) {
                            return rc;
                        }
                    }
                }
            }

            // Else if the key is not present in the node
            else {
                // If the node is not full
                if (numberKeys < keyCapacity) {
                    // Find the position for this key
                    int position = numberKeys;
                    for (int i=0; i<numberKeys; i++) {
                        if (givenKey < keyArray[i]) {
                            position = i;
                            break;
                        }
                    }

                    // Move the other keys forward and insert the key
                    for (int i=numberKeys; i>position; i--) {
                        keyArray[i] = keyArray[i-1];
                        valueArray[i] = valueArray[i-1];
                    }
                    keyArray[position] = givenKey;
                    valueArray[position].state = RID_FILLED;
                    valueArray[position].rid = rid;
                    valueArray[position].page = IX_NO_PAGE;

                    // Copy the keys and values to the node
                    memcpy(keyData, (char*) keyArray, attrLength*degree);
                    memcpy(valueData, (char*) valueArray, sizeof(IX_NodeValue)*(degree+1));
                }

                // If the node is full
                else {
                    // Allocate a new node page
                    PF_PageHandle newPFPH;
                    char* newPageData;
                    PageNum newPageNumber;
                    if ((rc = pfFH.AllocatePage(newPFPH))) {
                        return rc;
                    }
                    if ((rc = newPFPH.GetData(newPageData))) {
                        return rc;
                    }
                    if ((rc = newPFPH.GetPageNum(newPageNumber))) {
                        return rc;
                    }
                    if ((rc = pfFH.MarkDirty(newPageNumber))) {
                        return rc;
                    }

                    // Copy half the keys to the new page
                    IX_NodeHeader* newNodeHeader = new IX_NodeHeader;
                    string* newKeyArray = new string[degree];
                    IX_NodeValue* newValueArray = new IX_NodeValue[degree+1];
                    for (int i=numberKeys/2; i<numberKeys; i++) {
                        newKeyArray[i-numberKeys/2] = keyArray[i];
                        newValueArray[i-numberKeys/2] = valueArray[i];
                    }

                    // Update the node headers
                    nodeHeader->numberKeys = numberKeys/2;
                    nodeHeader->type = LEAF;
                    newNodeHeader->numberKeys = numberKeys - numberKeys/2;
                    newNodeHeader->keyCapacity = keyCapacity;
                    newNodeHeader->type = LEAF;

                    // Update the last pointer in the left node
                    valueArray[keyCapacity].state = PAGE_ONLY;
                    valueArray[keyCapacity].page = newPageNumber;
                    valueArray[keyCapacity].rid = dummyRID;

                    // Insert the new key
                    if (givenKey < newKeyArray[0]) {
                        // Insert in the left node
                        int position = numberKeys/2;
                        for (int i=0; i<numberKeys/2; i++) {
                            if (givenKey < keyArray[i]) {
                                position = i;
                                break;
                            }
                        }
                        for (int i=numberKeys/2; i>position; i--) {
                            keyArray[i] = keyArray[i-1];
                            valueArray[i] = valueArray[i-1];
                        }
                        keyArray[position] = givenKey;
                        valueArray[position].state = RID_FILLED;
                        valueArray[position].rid = rid;
                        valueArray[position].page = IX_NO_PAGE;

                        nodeHeader->numberKeys++;
                    }
                    else {
                        // Insert in the right node
                        int position = numberKeys - numberKeys/2;
                        for (int i=0; i< numberKeys - numberKeys/2; i++) {
                            if (givenKey < newKeyArray[i]) {
                                position = i;
                                break;
                            }
                        }
                        for (int i=numberKeys-numberKeys/2; i>position; i--) {
                            newKeyArray[i] = newKeyArray[i-1];
                            newValueArray[i] = newValueArray[i-1];
                        }
                        newKeyArray[position] = givenKey;
                        newValueArray[position].state = RID_FILLED;
                        newValueArray[position].rid = rid;
                        newValueArray[position].page = IX_NO_PAGE;

                        newNodeHeader->numberKeys++;
                    }

                    // Get the parent node
                    PageNum parentNode = nodeHeader->parent;
                    newNodeHeader->parent = parentNode;

                    // Call the pushKeyUp function to push the new key up
                    if ((rc = pushKeyUp((void*) newKeyArray, parentNode, node, newPageNumber))) {
                        return rc;
                    }

                    // Unpin the new page
                    if ((rc = pfFH.UnpinPage(newPageNumber))) {
                        return rc;
                    }
                }
            }
        }

        // Unpin the node page
        if ((rc = pfFH.UnpinPage(node))) {
            return rc;
        }

        // Return OK
        return OK_RC;
    }

    // Else if the type is ROOT / NODE
    else {
        PageNum nextNode;
        if (attrType == INT) {
            int* keyArray = (int*) keyData;
            int givenKey = *static_cast<int*>(pData);

            // Search for corresponding pointer to next node
            int position = numberKeys;
            for (int i=0; i<numberKeys; i++) {
                if (givenKey < keyArray[i]) {
                    position = i;
                    break;
                }
            }
            nextNode = valueArray[position].page;
        }
        else if (attrType == FLOAT) {
            float* keyArray = (float*) keyData;
            float givenKey = *static_cast<float*>(pData);

            // Search for corresponding pointer to next node
            int position = numberKeys;
            for (int i=0; i<numberKeys; i++) {
                if (givenKey < keyArray[i]) {
                    position = i;
                    break;
                }
            }
            nextNode = valueArray[position].page;
        }
        else {
            string* keyArray = (string*) keyData;
            char* givenKeyChar = static_cast<char*>(pData);
            string givenKey = "";
            for (int i=0; i < indexHeader.attrLength; i++) {
                givenKey += givenKeyChar[i];
            }

            // Search for corresponding pointer to next node
            int position = numberKeys;
            for (int i=0; i<numberKeys; i++) {
                if (givenKey < keyArray[i]) {
                    position = i;
                    break;
                }
            }
            nextNode = valueArray[position].page;
        }

        // Make recursive call to the next node
        if ((rc = InsertEntryRecursive(pData, rid, nextNode))) {
            return rc;
        }

        // Unpin the page
        if ((rc = pfFH.UnpinPage(node))) {
            return rc;
        }

        // Return OK
        return OK_RC;
    }
}

// Method: pushKeyUp(void* pData, PageNum node)
// Push a key up to the parent during split
/* Steps:
    1) Get the node page
    2) If node is not full, insert new key in the node
    3) If node is full,
        - Allocate a new node page
        - Split key-values in the 2 pages
        - Update the parent pointers of all the children
        - Set the types to NODE
        - Get the parent of the left page
    4) If the parent is IX_NO_PAGE
        - Allocate new page and set its type as ROOT
        - Set parents of left and right as the new page
        - Push the first key from right to the new root
        - Set the pointers in the new root
        - Update the root page in the index header
        - Unpin all the pages
    5) Else if parent exists
        - Make the recursive call with the parent node and key
        - Unpin the pages
    6) Copy the modified data to the pages
    7) Unpin node page and return OK
*/
RC IX_IndexHandle::pushKeyUp(void* pData, PageNum node, PageNum left, PageNum right) {
    // Declare an integer for the return code
    int rc;

    // Get the node page
    PF_PageHandle pfPH;
    char* nodeData;
    if ((rc = pfFH.GetThisPage(node, pfPH))) {
        return rc;
    }
    if ((rc = pfPH.GetData(nodeData))) {
        return rc;
    }
    if ((rc = pfFH.MarkDirty(node))) {
        return rc;
    }

    AttrType attrType = indexHeader.attrType;
    int attrLength = indexHeader.attrLength;
    int degree = indexHeader.degree;

    IX_NodeHeader* nodeHeader = (IX_NodeHeader*) nodeData;
    char* keyData = nodeData + sizeof(IX_NodeHeader);
    char* valueData = keyData + attrLength*degree;
    IX_NodeValue* valueArray = (IX_NodeValue*) valueData;

    // Get the information from the node header
    int numberKeys = nodeHeader->numberKeys;
    int keyCapacity = nodeHeader->keyCapacity;

    if (attrType == INT) {
        int* keyArray = (int*) keyData;
        int givenKey = *static_cast<int*>(pData);

        // If the node is not full
        if (numberKeys < keyCapacity) {
            // Find the position for this key
            int position = numberKeys;
            for (int i=0; i<numberKeys; i++) {
                if (givenKey < keyArray[i]) {
                    position = i;
                    break;
                }
            }

            // Move the other keys forward and insert the key
            for (int i=numberKeys; i>position; i--) {
                keyArray[i] = keyArray[i-1];
                valueArray[i] = valueArray[i-1];
            }
            keyArray[position] = givenKey;
            valueArray[position].state = PAGE_ONLY;
            valueArray[position].page = left;
            valueArray[position].rid = dummyRID;
            valueArray[position+1].page = right;

            // Copy the keys and values to the node
            memcpy(keyData, (char*) keyArray, attrLength*degree);
            memcpy(valueData, (char*) valueArray, sizeof(IX_NodeValue)*(degree+1));
        }

        // Else if the node is full
        else {
            // Allocate a new node page
            PF_PageHandle newPFPH;
            char* newPageData;
            PageNum newPageNumber;
            if ((rc = pfFH.AllocatePage(newPFPH))) {
                return rc;
            }
            if ((rc = newPFPH.GetData(newPageData))) {
                return rc;
            }
            if ((rc = newPFPH.GetPageNum(newPageNumber))) {
                return rc;
            }
            if ((rc = pfFH.MarkDirty(newPageNumber))) {
                return rc;
            }

            // Copy half the keys to the new page
            IX_NodeHeader* newNodeHeader = new IX_NodeHeader;
            int* newKeyArray = new int[degree];
            IX_NodeValue* newValueArray = new IX_NodeValue[degree+1];
            for (int i=numberKeys/2; i<numberKeys; i++) {
                newKeyArray[i-numberKeys/2] = keyArray[i];
                newValueArray[i-numberKeys/2] = valueArray[i];
            }

            // Update the node headers
            nodeHeader->numberKeys = numberKeys/2;
            nodeHeader->type = NODE;
            newNodeHeader->numberKeys = numberKeys - numberKeys/2;
            newNodeHeader->keyCapacity = keyCapacity;
            newNodeHeader->type = NODE;

            // Update the last pointer in the left node
            valueArray[keyCapacity].state = PAGE_ONLY;
            valueArray[keyCapacity].page = newPageNumber;
            valueArray[keyCapacity].rid = dummyRID;

            // Insert the new key
            if (givenKey < newKeyArray[0]) {
                // Insert in the left node
                int position = numberKeys/2;
                for (int i=0; i<numberKeys/2; i++) {
                    if (givenKey < keyArray[i]) {
                        position = i;
                        break;
                    }
                }
                for (int i=numberKeys/2; i>position; i--) {
                    keyArray[i] = keyArray[i-1];
                    valueArray[i] = valueArray[i-1];
                }
                keyArray[position] = givenKey;
                valueArray[position].state = PAGE_ONLY;
                valueArray[position].page = left;
                valueArray[position].rid = dummyRID;
                valueArray[position+1].page = right;

                nodeHeader->numberKeys++;
            }
            else {
                // Insert in the right node
                int position = numberKeys - numberKeys/2;
                for (int i=0; i< numberKeys - numberKeys/2; i++) {
                    if (givenKey < newKeyArray[i]) {
                        position = i;
                        break;
                    }
                }
                for (int i=numberKeys-numberKeys/2; i>position; i--) {
                    newKeyArray[i] = newKeyArray[i-1];
                    newValueArray[i] = newValueArray[i-1];
                }
                newKeyArray[position] = givenKey;
                newValueArray[position].state = PAGE_ONLY;
                newValueArray[position].page = left;
                newValueArray[position].rid = dummyRID;
                newValueArray[position+1].page = right;

                newNodeHeader->numberKeys++;
            }

            // Remove the first key from the right node
            int keyToPushUp = newKeyArray[0];
            for (int i=0; i<newNodeHeader->numberKeys; i++) {
                newKeyArray[i] = newKeyArray[i+1];
                newValueArray[i] = newValueArray[i+1];
            }
            newNodeHeader->numberKeys--;

            // Update the parent pointers of the children
            PageNum childPage;
            PF_PageHandle childPFPH;
            char* childData;
            for (int i=0; i<=newNodeHeader->numberKeys; i++) {
                childPage = newValueArray[i].page;
                if ((rc = pfFH.GetThisPage(childPage, childPFPH))) {
                    return rc;
                }
                if ((rc = childPFPH.GetData(childData))) {
                    return rc;
                }
                if ((rc = pfFH.MarkDirty(childPage))) {
                    return rc;
                }
                IX_NodeHeader* childHeader = (IX_NodeHeader*) childData;
                childHeader->parent = newPageNumber;
                memcpy(childData, (char*) childHeader, sizeof(IX_NodeHeader));

                if ((rc = pfFH.UnpinPage(childPage))) {
                    return rc;
                }
            }

            // Get the parent node
            PageNum parentNode = nodeHeader->parent;

            // If parent does not exist
            if (parentNode == IX_NO_PAGE) {
                // Allocate a new root page
                PF_PageHandle newRootPFPH;
                char* newRootPageData;
                PageNum newRootPage;
                if ((rc = pfFH.AllocatePage(newRootPFPH))) {
                    return rc;
                }
                if ((rc = newRootPFPH.GetData(newRootPageData))) {
                    return rc;
                }
                if ((rc = newRootPFPH.GetPageNum(newRootPage))) {
                    return rc;
                }
                if ((rc = pfFH.MarkDirty(newRootPage))) {
                    return rc;
                }

                // Set the keys and values
                IX_NodeHeader* newRootNodeHeader = new IX_NodeHeader;
                int* newRootKeyArray = new int[degree];
                IX_NodeValue* newRootValueArray = new IX_NodeValue[degree+1];
                newRootNodeHeader->numberKeys = 1;
                newRootNodeHeader->keyCapacity = keyCapacity;
                newRootNodeHeader->type = ROOT;
                newRootNodeHeader->parent = IX_NO_PAGE;

                newRootKeyArray[0] = keyToPushUp;
                newRootValueArray[0].state = PAGE_ONLY;
                newRootValueArray[0].page = node;
                newRootValueArray[0].rid = dummyRID;
                newRootValueArray[1].state = PAGE_ONLY;
                newRootValueArray[1].page = newPageNumber;
                newRootValueArray[1].rid = dummyRID;

                // Copy the data to the root page
                memcpy(newRootPageData, (char*) newRootNodeHeader, sizeof(IX_NodeHeader));
                memcpy(newRootPageData+sizeof(IX_NodeHeader), (char*) newRootKeyArray, attrLength*degree);
                memcpy(newRootPageData+sizeof(IX_NodeHeader)+attrLength*degree, (char*) newRootValueArray, sizeof(IX_NodeValue)*(degree+1));
                delete newRootNodeHeader;
                delete[] newRootKeyArray;
                delete[] newRootValueArray;

                // Update the parent pointers
                nodeHeader->parent = newRootPage;
                newNodeHeader->parent = newRootPage;

                // Update the root page in the index header
                indexHeader.rootPage = newRootPage;
                headerModified = TRUE;

                // Unpin the root page
                if ((rc = pfFH.UnpinPage(newRootPage))) {
                    return rc;
                }
            }

            // Else if parent exists
            else {
                // Make recursive call with the parent node
                newNodeHeader->parent = parentNode;
                if ((rc = pushKeyUp((void*) &keyToPushUp, parentNode, node, newPageNumber))) {
                    return rc;
                }
            }

            // Copy the data to the pages
            memcpy(nodeData, (char*) nodeHeader, sizeof(IX_NodeHeader));
            memcpy(keyData, (char*) keyArray, attrLength*degree);
            memcpy(valueData, (char*) valueArray, sizeof(IX_NodeValue)*(degree+1));

            memcpy(newPageData, (char*) newNodeHeader, sizeof(IX_NodeHeader));
            memcpy(newPageData+sizeof(IX_NodeHeader), (char*) newKeyArray, attrLength*degree);
            memcpy(newPageData+sizeof(IX_NodeHeader)+attrLength*degree, (char*) newValueArray, sizeof(IX_NodeValue)*(degree+1));
            delete newNodeHeader;
            delete[] newKeyArray;
            delete[] newValueArray;

            // Unpin the new page
            if ((rc = pfFH.UnpinPage(newPageNumber))) {
                return rc;
            }
        }
    }

    else if (attrType == FLOAT) {
        float* keyArray = (float*) keyData;
        float givenKey = *static_cast<float*>(pData);

        // If the node is not full
        if (numberKeys < keyCapacity) {
            // Find the position for this key
            int position = numberKeys;
            for (int i=0; i<numberKeys; i++) {
                if (givenKey < keyArray[i]) {
                    position = i;
                    break;
                }
            }

            // Move the other keys forward and insert the key
            for (int i=numberKeys; i>position; i--) {
                keyArray[i] = keyArray[i-1];
                valueArray[i] = valueArray[i-1];
            }
            keyArray[position] = givenKey;
            valueArray[position].state = PAGE_ONLY;
            valueArray[position].page = left;
            valueArray[position].rid = dummyRID;
            valueArray[position+1].page = right;

            // Copy the keys and values to the node
            memcpy(keyData, (char*) keyArray, attrLength*degree);
            memcpy(valueData, (char*) valueArray, sizeof(IX_NodeValue)*(degree+1));
        }

        // Else if the node is full
        else {
            // Allocate a new node page
            PF_PageHandle newPFPH;
            char* newPageData;
            PageNum newPageNumber;
            if ((rc = pfFH.AllocatePage(newPFPH))) {
                return rc;
            }
            if ((rc = newPFPH.GetData(newPageData))) {
                return rc;
            }
            if ((rc = newPFPH.GetPageNum(newPageNumber))) {
                return rc;
            }
            if ((rc = pfFH.MarkDirty(newPageNumber))) {
                return rc;
            }

            // Copy half the keys to the new page
            IX_NodeHeader* newNodeHeader = new IX_NodeHeader;
            float* newKeyArray = new float[degree];
            IX_NodeValue* newValueArray = new IX_NodeValue[degree+1];
            for (int i=numberKeys/2; i<numberKeys; i++) {
                newKeyArray[i-numberKeys/2] = keyArray[i];
                newValueArray[i-numberKeys/2] = valueArray[i];
            }

            // Update the node headers
            nodeHeader->numberKeys = numberKeys/2;
            nodeHeader->type = NODE;
            newNodeHeader->numberKeys = numberKeys - numberKeys/2;
            newNodeHeader->keyCapacity = keyCapacity;
            newNodeHeader->type = NODE;

            // Update the last pointer in the left node
            valueArray[keyCapacity].state = PAGE_ONLY;
            valueArray[keyCapacity].page = newPageNumber;
            valueArray[keyCapacity].rid = dummyRID;

            // Insert the new key
            if (givenKey < newKeyArray[0]) {
                // Insert in the left node
                int position = numberKeys/2;
                for (int i=0; i<numberKeys/2; i++) {
                    if (givenKey < keyArray[i]) {
                        position = i;
                        break;
                    }
                }
                for (int i=numberKeys/2; i>position; i--) {
                    keyArray[i] = keyArray[i-1];
                    valueArray[i] = valueArray[i-1];
                }
                keyArray[position] = givenKey;
                valueArray[position].state = PAGE_ONLY;
                valueArray[position].page = left;
                valueArray[position].rid = dummyRID;
                valueArray[position+1].page = right;

                nodeHeader->numberKeys++;
            }
            else {
                // Insert in the right node
                int position = numberKeys - numberKeys/2;
                for (int i=0; i< numberKeys - numberKeys/2; i++) {
                    if (givenKey < newKeyArray[i]) {
                        position = i;
                        break;
                    }
                }
                for (int i=numberKeys-numberKeys/2; i>position; i--) {
                    newKeyArray[i] = newKeyArray[i-1];
                    newValueArray[i] = newValueArray[i-1];
                }
                newKeyArray[position] = givenKey;
                newValueArray[position].state = PAGE_ONLY;
                newValueArray[position].page = left;
                newValueArray[position].rid = dummyRID;
                newValueArray[position+1].page = right;

                newNodeHeader->numberKeys++;
            }

            // Remove the first key from the right node
            float keyToPushUp = newKeyArray[0];
            for (int i=0; i<newNodeHeader->numberKeys; i++) {
                newKeyArray[i] = newKeyArray[i+1];
                newValueArray[i] = newValueArray[i+1];
            }
            newNodeHeader->numberKeys--;

            // Update the parent pointers of the children
            PageNum childPage;
            PF_PageHandle childPFPH;
            char* childData;
            for (int i=0; i<=newNodeHeader->numberKeys; i++) {
                childPage = newValueArray[i].page;
                if ((rc = pfFH.GetThisPage(childPage, childPFPH))) {
                    return rc;
                }
                if ((rc = childPFPH.GetData(childData))) {
                    return rc;
                }
                if ((rc = pfFH.MarkDirty(childPage))) {
                    return rc;
                }
                IX_NodeHeader* childHeader = (IX_NodeHeader*) childData;
                childHeader->parent = newPageNumber;
                memcpy(childData, (char*) childHeader, sizeof(IX_NodeHeader));

                if ((rc = pfFH.UnpinPage(childPage))) {
                    return rc;
                }
            }

            // Get the parent node
            PageNum parentNode = nodeHeader->parent;

            // If parent does not exist
            if (parentNode == IX_NO_PAGE) {
                // Allocate a new root page
                PF_PageHandle newRootPFPH;
                char* newRootPageData;
                PageNum newRootPage;
                if ((rc = pfFH.AllocatePage(newRootPFPH))) {
                    return rc;
                }
                if ((rc = newRootPFPH.GetData(newRootPageData))) {
                    return rc;
                }
                if ((rc = newRootPFPH.GetPageNum(newRootPage))) {
                    return rc;
                }
                if ((rc = pfFH.MarkDirty(newRootPage))) {
                    return rc;
                }

                // Set the keys and values
                IX_NodeHeader* newRootNodeHeader = new IX_NodeHeader;
                float* newRootKeyArray = new float[degree];
                IX_NodeValue* newRootValueArray = new IX_NodeValue[degree+1];
                newRootNodeHeader->numberKeys = 1;
                newRootNodeHeader->keyCapacity = keyCapacity;
                newRootNodeHeader->type = ROOT;
                newRootNodeHeader->parent = IX_NO_PAGE;

                newRootKeyArray[0] = keyToPushUp;
                newRootValueArray[0].state = PAGE_ONLY;
                newRootValueArray[0].page = node;
                newRootValueArray[0].rid = dummyRID;
                newRootValueArray[1].state = PAGE_ONLY;
                newRootValueArray[1].page = newPageNumber;
                newRootValueArray[1].rid = dummyRID;

                // Copy the data to the root page
                memcpy(newRootPageData, (char*) newRootNodeHeader, sizeof(IX_NodeHeader));
                memcpy(newRootPageData+sizeof(IX_NodeHeader), (char*) newRootKeyArray, attrLength*degree);
                memcpy(newRootPageData+sizeof(IX_NodeHeader)+attrLength*degree, (char*) newRootValueArray, sizeof(IX_NodeValue)*(degree+1));
                delete newRootNodeHeader;
                delete[] newRootKeyArray;
                delete[] newRootValueArray;

                // Update the parent pointers
                nodeHeader->parent = newRootPage;
                newNodeHeader->parent = newRootPage;

                // Update the root page in the index header
                indexHeader.rootPage = newRootPage;
                headerModified = TRUE;

                // Unpin the root page
                if ((rc = pfFH.UnpinPage(newRootPage))) {
                    return rc;
                }
            }

            // Else if parent exists
            else {
                // Make recursive call with the parent node
                newNodeHeader->parent = parentNode;
                if ((rc = pushKeyUp((void*) &keyToPushUp, parentNode, node, newPageNumber))) {
                    return rc;
                }
            }

            // Copy the data to the pages
            memcpy(nodeData, (char*) nodeHeader, sizeof(IX_NodeHeader));
            memcpy(keyData, (char*) keyArray, attrLength*degree);
            memcpy(valueData, (char*) valueArray, sizeof(IX_NodeValue)*(degree+1));

            memcpy(newPageData, (char*) newNodeHeader, sizeof(IX_NodeHeader));
            memcpy(newPageData+sizeof(IX_NodeHeader), (char*) newKeyArray, attrLength*degree);
            memcpy(newPageData+sizeof(IX_NodeHeader)+attrLength*degree, (char*) newValueArray, sizeof(IX_NodeValue)*(degree+1));
            delete newNodeHeader;
            delete[] newKeyArray;
            delete[] newValueArray;

            // Unpin the new page
            if ((rc = pfFH.UnpinPage(newPageNumber))) {
                return rc;
            }
        }
    }

    else {
        string* keyArray = (string*) keyData;
        char* givenKeyChar = static_cast<char*>(pData);
        string givenKey = "";
        for (int i=0; i < indexHeader.attrLength; i++) {
            givenKey += givenKeyChar[i];
        }

        // If the node is not full
        if (numberKeys < keyCapacity) {
            // Find the position for this key
            int position = numberKeys;
            for (int i=0; i<numberKeys; i++) {
                if (givenKey < keyArray[i]) {
                    position = i;
                    break;
                }
            }

            // Move the other keys forward and insert the key
            for (int i=numberKeys; i>position; i--) {
                keyArray[i] = keyArray[i-1];
                valueArray[i] = valueArray[i-1];
            }
            keyArray[position] = givenKey;
            valueArray[position].state = PAGE_ONLY;
            valueArray[position].page = left;
            valueArray[position].rid = dummyRID;
            valueArray[position+1].page = right;

            // Copy the keys and values to the node
            memcpy(keyData, (char*) keyArray, attrLength*degree);
            memcpy(valueData, (char*) valueArray, sizeof(IX_NodeValue)*(degree+1));
        }

        // Else if the node is full
        else {
            // Allocate a new node page
            PF_PageHandle newPFPH;
            char* newPageData;
            PageNum newPageNumber;
            if ((rc = pfFH.AllocatePage(newPFPH))) {
                return rc;
            }
            if ((rc = newPFPH.GetData(newPageData))) {
                return rc;
            }
            if ((rc = newPFPH.GetPageNum(newPageNumber))) {
                return rc;
            }
            if ((rc = pfFH.MarkDirty(newPageNumber))) {
                return rc;
            }

            // Copy half the keys to the new page
            IX_NodeHeader* newNodeHeader = new IX_NodeHeader;
            string* newKeyArray = new string[degree];
            IX_NodeValue* newValueArray = new IX_NodeValue[degree+1];
            for (int i=numberKeys/2; i<numberKeys; i++) {
                newKeyArray[i-numberKeys/2] = keyArray[i];
                newValueArray[i-numberKeys/2] = valueArray[i];
            }

            // Update the node headers
            nodeHeader->numberKeys = numberKeys/2;
            nodeHeader->type = NODE;
            newNodeHeader->numberKeys = numberKeys - numberKeys/2;
            newNodeHeader->keyCapacity = keyCapacity;
            newNodeHeader->type = NODE;

            // Update the last pointer in the left node
            valueArray[keyCapacity].state = PAGE_ONLY;
            valueArray[keyCapacity].page = newPageNumber;
            valueArray[keyCapacity].rid = dummyRID;

            // Insert the new key
            if (givenKey < newKeyArray[0]) {
                // Insert in the left node
                int position = numberKeys/2;
                for (int i=0; i<numberKeys/2; i++) {
                    if (givenKey < keyArray[i]) {
                        position = i;
                        break;
                    }
                }
                for (int i=numberKeys/2; i>position; i--) {
                    keyArray[i] = keyArray[i-1];
                    valueArray[i] = valueArray[i-1];
                }
                keyArray[position] = givenKey;
                valueArray[position].state = PAGE_ONLY;
                valueArray[position].page = left;
                valueArray[position].rid = dummyRID;
                valueArray[position+1].page = right;

                nodeHeader->numberKeys++;
            }
            else {
                // Insert in the right node
                int position = numberKeys - numberKeys/2;
                for (int i=0; i< numberKeys - numberKeys/2; i++) {
                    if (givenKey < newKeyArray[i]) {
                        position = i;
                        break;
                    }
                }
                for (int i=numberKeys-numberKeys/2; i>position; i--) {
                    newKeyArray[i] = newKeyArray[i-1];
                    newValueArray[i] = newValueArray[i-1];
                }
                newKeyArray[position] = givenKey;
                newValueArray[position].state = PAGE_ONLY;
                newValueArray[position].page = left;
                newValueArray[position].rid = dummyRID;
                newValueArray[position+1].page = right;

                newNodeHeader->numberKeys++;
            }

            // Remove the first key from the right node
            string keyToPushUp = newKeyArray[0];
            for (int i=0; i<newNodeHeader->numberKeys; i++) {
                newKeyArray[i] = newKeyArray[i+1];
                newValueArray[i] = newValueArray[i+1];
            }
            newNodeHeader->numberKeys--;

            // Update the parent pointers of the children
            PageNum childPage;
            PF_PageHandle childPFPH;
            char* childData;
            for (int i=0; i<=newNodeHeader->numberKeys; i++) {
                childPage = newValueArray[i].page;
                if ((rc = pfFH.GetThisPage(childPage, childPFPH))) {
                    return rc;
                }
                if ((rc = childPFPH.GetData(childData))) {
                    return rc;
                }
                if ((rc = pfFH.MarkDirty(childPage))) {
                    return rc;
                }
                IX_NodeHeader* childHeader = (IX_NodeHeader*) childData;
                childHeader->parent = newPageNumber;
                memcpy(childData, (char*) childHeader, sizeof(IX_NodeHeader));

                if ((rc = pfFH.UnpinPage(childPage))) {
                    return rc;
                }
            }

            // Get the parent node
            PageNum parentNode = nodeHeader->parent;

            // If parent does not exist
            if (parentNode == IX_NO_PAGE) {
                // Allocate a new root page
                PF_PageHandle newRootPFPH;
                char* newRootPageData;
                PageNum newRootPage;
                if ((rc = pfFH.AllocatePage(newRootPFPH))) {
                    return rc;
                }
                if ((rc = newRootPFPH.GetData(newRootPageData))) {
                    return rc;
                }
                if ((rc = newRootPFPH.GetPageNum(newRootPage))) {
                    return rc;
                }
                if ((rc = pfFH.MarkDirty(newRootPage))) {
                    return rc;
                }

                // Set the keys and values
                IX_NodeHeader* newRootNodeHeader = new IX_NodeHeader;
                string* newRootKeyArray = new string[degree];
                IX_NodeValue* newRootValueArray = new IX_NodeValue[degree+1];
                newRootNodeHeader->numberKeys = 1;
                newRootNodeHeader->keyCapacity = keyCapacity;
                newRootNodeHeader->type = ROOT;
                newRootNodeHeader->parent = IX_NO_PAGE;

                newRootKeyArray[0] = keyToPushUp;
                newRootValueArray[0].state = PAGE_ONLY;
                newRootValueArray[0].page = node;
                newRootValueArray[0].rid = dummyRID;
                newRootValueArray[1].state = PAGE_ONLY;
                newRootValueArray[1].page = newPageNumber;
                newRootValueArray[1].rid = dummyRID;

                // Copy the data to the root page
                memcpy(newRootPageData, (char*) newRootNodeHeader, sizeof(IX_NodeHeader));
                memcpy(newRootPageData+sizeof(IX_NodeHeader), (char*) newRootKeyArray, attrLength*degree);
                memcpy(newRootPageData+sizeof(IX_NodeHeader)+attrLength*degree, (char*) newRootValueArray, sizeof(IX_NodeValue)*(degree+1));
                delete newRootNodeHeader;
                delete[] newRootKeyArray;
                delete[] newRootValueArray;

                // Update the parent pointers
                nodeHeader->parent = newRootPage;
                newNodeHeader->parent = newRootPage;

                // Update the root page in the index header
                indexHeader.rootPage = newRootPage;
                headerModified = TRUE;

                // Unpin the root page
                if ((rc = pfFH.UnpinPage(newRootPage))) {
                    return rc;
                }
            }

            // Else if parent exists
            else {
                // Make recursive call with the parent node
                newNodeHeader->parent = parentNode;
                if ((rc = pushKeyUp((void*) &keyToPushUp, parentNode, node, newPageNumber))) {
                    return rc;
                }
            }

            // Copy the data to the pages
            memcpy(nodeData, (char*) nodeHeader, sizeof(IX_NodeHeader));
            memcpy(keyData, (char*) keyArray, attrLength*degree);
            memcpy(valueData, (char*) valueArray, sizeof(IX_NodeValue)*(degree+1));

            memcpy(newPageData, (char*) newNodeHeader, sizeof(IX_NodeHeader));
            memcpy(newPageData+sizeof(IX_NodeHeader), (char*) newKeyArray, attrLength*degree);
            memcpy(newPageData+sizeof(IX_NodeHeader)+attrLength*degree, (char*) newValueArray, sizeof(IX_NodeValue)*(degree+1));
            delete newNodeHeader;
            delete[] newKeyArray;
            delete[] newValueArray;

            // Unpin the new page
            if ((rc = pfFH.UnpinPage(newPageNumber))) {
                return rc;
            }
        }
    }

    // Unpin the node page
    if ((rc = pfFH.UnpinPage(node))) {
        return rc;
    }

    // Return OK
    return OK_RC;
}

// Method: DeleteEntry(void *pData, const RID &rid)
// Delete a new index entry
/* Steps:

*/
RC IX_IndexHandle::DeleteEntry(void *pData, const RID &rid) {
    return 0;
}

// Method: ForcePages()
// Force index pages to disk
RC IX_IndexHandle::ForcePages() {
    // Declare an integer for the return code
    int rc;

    // Get the first page
    PageNum pageNum;
    PF_PageHandle pfPH;
    if ((rc = pfFH.GetFirstPage(pfPH))) {
        return rc;
    }
    if ((rc = pfPH.GetPageNum(pageNum))) {
        return rc;
    }

    // Get next page number
    PageNum nextPage;
    PF_PageHandle nextPFPH;
    while (true) {
        if ((rc = pfFH.GetNextPage(pageNum, nextPFPH))) {
            return rc;
        }
        if ((rc = pfFH.UnpinPage(pageNum))) {
            return rc;
        }

        // Force the pages using the PF FileHandle
        if ((rc = pfFH.ForcePages(pageNum))) {
            return rc;
        }

        if ((rc = nextPFPH.GetPageNum(nextPage))) {
            if (rc == PF_EOF) {
                break;
            }
            return rc;
        }

        pageNum = nextPage;
    }

    // Return OK
    return OK_RC;
}


// // Method: SearchEntry(void *pData, const RID &rid)
// // Search for an entry in the B+ Tree index
// /* Steps:
//     1) Get the root page number from the index header
//     2) Call the recursive search function with the root page number
// */
// RC IX_IndexHandle::SearchEntry(void *pData, const RID &rid) {
//     // Declare an integer for the return code
//     int rc;

//     // Call the recursive function with the root page
//     PageNum rootPage = indexHeader.rootPage;
//     if ((rc = SearchEntryRecursive(pData, rid, rootPage))) {
//         return rc;
//     }

//     // Return OK
//     return OK_RC;
// }

// // Method: SearchEntryRecursive(void* pData, RID &rid, PageNum node)
// // Recursively search for the entry in the tree
// /* Steps:
//     1) Get the data in the node page
//     2) Check type of node in the node header
//     3) If leaf node
//         - Search for the value of pData in the node
//         - Get the corresponding bucket page/RID
//         - Unpin the current node page
//         - Return the bucket page number
//     4) If root/internal node
//         - Search for corresponding pointer to next node
//         - Unpin the current node page
//         - Recursive call with the next node page number
// */
// RC IX_IndexHandle::SearchEntryRecursive(void *pData, RID &rid, PageNum node) {
//     // Declare an integer for the return code
//     int rc;

//     // Get the data in the node page
//     PF_PageHandle pfPH;
//     if ((rc = pfFH.GetThisPage(node, pfPH))) {
//         // Return the error from the PF FileHandle
//         return rc;
//     }
//     char* nodeData;
//     if ((rc = pfPH.GetData(nodeData))) {
//         // Return the error from the PF PageHandle
//         return rc;
//     }

//     // Get the node type
//     IX_NodeHeader* nodeHeader = (IX_NodeHeader*) nodeData;
//     IX_NodeType nodeType = nodeHeader->type;
//     int numberKeys = nodeHeader.numberKeys;
//     int attrLength = indexHeader.attrLength;
//     AttrType attrType = indexHeader.attrType;

//     // If leaf node
//     // Search for the value of pData in the node
//     if (nodeType == LEAF) {
//         // Start at the first key
//         char* keyData = nodeData + sizeof(IX_NodeHeader);

//         // Iterate over the number of keys
//         for (int i=0; i<numberKeys; i++) {
//             if (equalAttribute(attrType, keyData, pData)) {
//                 int valueOffset = sizeof(IX_NodeHeader) + (nodeHeader.keyCapacity)*attrLength + i*sizeof(IX_NodeValue);
//                 IX_NodeValue* nodeValue = (IX_NodeValue*) nodeData + valueOffset;
//                 // TODO: Return the bucket page number / RID?

//                 // Unpin the page
//                 if ((rc = pfFH.UnpinPage(node))) {
//                     // Return the error from the PF File Handle
//                     return rc;
//                 }

//                 // Return OK
//                 return OK_RC;
//             }
//             keyData += attrLength;
//         }

//         // Unpin the page
//         if ((rc = pfFH.UnpinPage(node))) {
//             // Return the error from the PF File Handle
//             return rc;
//         }

//         // Key was not found
//         return IX_KEY_NOT_FOUND;
//     }

//     // Else if root/internal node
//     // Search for the key greater than pData in the node
//     else {
//         // Start at the first key
//         char* keyData = nodeData + sizeof(IX_NodeHeader);
//         bool found = false;

//         // Iterate over the number of keys
//         for (int i=0; i<numberKeys; i++) {
//             if (largerAttribute(attrType, keyData, pData)) {
//                 int valueOffset = sizeof(IX_NodeHeader) + (nodeHeader.keyCapacity)*attrLength + i*sizeof(IX_NodeValue);
//                 IX_NodeValue* nodeValue = (IX_NodeValue*) nodeData + valueOffset;

//                 // Check that it is a page pointer only
//                 if (nodeValue->state != PAGE_ONLY) {
//                     return IX_INCONSISTENT_NODE;
//                 }

//                 // Get the next page pointer
//                 PageNum nextPage = nodeValue->page;

//                 // Unpin the page
//                 if ((rc = pfFH.UnpinPage(node))) {
//                     // Return the error from the PF File Handle
//                     return rc;
//                 }

//                 // Make the recursive call with the next page number
//                 return SearchEntryRecursive(pData, rid, nextPage);
//             }
//         }
//     }
// }


// Method: bool equalAttribute(AttrType attrType, char* nodeData, void* pData)
// Compare attribute values and return a boolean
bool IX_IndexHandle::equalAttribute(AttrType attrType, char* nodeData, void* pData) {
    if (attrType == INT) {
        int nodeKey = getIntegerValue(nodeData);
        int givenKey = *static_cast<int*>(pData);
        return (nodeKey == givenKey);
    }
    else if (attrType == FLOAT) {
        float nodeKey = getFloatValue(nodeData);
        float givenKey = *static_cast<float*>(pData);
        return (nodeKey == givenKey);
    }
    else {
        string nodeKey = getStringValue(nodeData);
        char* givenKeyChar = static_cast<char*>(pData);
        string givenKey = "";
        for (int i=0; i < indexHeader.attrLength; i++) {
            givenKey += givenKeyChar[i];
        }
        return (nodeKey == givenKey);
    }
}

// Method: bool largerAttribute(AttrType attrType, char* nodeData, void* pData)
// Compare attribute values and return a boolean
bool IX_IndexHandle::largerAttribute(AttrType attrType, char* nodeData, void* pData) {
    if (attrType == INT) {
        int nodeKey = getIntegerValue(nodeData);
        int givenKey = *static_cast<int*>(pData);
        return (nodeKey > givenKey);
    }
    else if (attrType == FLOAT) {
        float nodeKey = getFloatValue(nodeData);
        float givenKey = *static_cast<float*>(pData);
        return (nodeKey > givenKey);
    }
    else {
        string nodeKey = getStringValue(nodeData);
        char* givenKeyChar = static_cast<char*>(pData);
        string givenKey = "";
        for (int i=0; i < indexHeader.attrLength; i++) {
            givenKey += givenKeyChar[i];
        }
        return (nodeKey > givenKey);
    }
}

// Method: int getIntegerValue(char* data)
// Get integer attribute value
int IX_IndexHandle::getIntegerValue(char* data) {
    int recordValue;
    memcpy(&recordValue, data, sizeof(recordValue));
    return recordValue;
}

// Method: float getFloatValue(char* data)
// Get float attribute value
float IX_IndexHandle::getFloatValue(char* data) {
    float recordValue;
    memcpy(&recordValue, data, sizeof(recordValue));
    return recordValue;
}

// Method: string getStringValue(char* data)
// Get string attribute value
string IX_IndexHandle::getStringValue(char* data) {
    string recordValue = "";
    for (int i=0; i < indexHeader.attrLength; i++) {
        recordValue += data[i];
    }
    return recordValue;
}

// Method: compareRIDs(RID &rid1, RID &rid2)
// Boolean whether the two RIDs are the same
bool IX_IndexHandle::compareRIDs(const RID &rid1, const RID &rid2) {
    PageNum pageNum1, pageNum2;
    SlotNum slotNum1, slotNum2;

    rid1.GetPageNum(pageNum1);
    rid1.GetSlotNum(slotNum1);
    rid2.GetPageNum(pageNum2);
    rid2.GetSlotNum(slotNum2);

    return (pageNum1 == pageNum2 && slotNum1 == slotNum2);
}