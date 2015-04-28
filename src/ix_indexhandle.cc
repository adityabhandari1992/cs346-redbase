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


/************** CODE FOR INSERT ENTRY *****************/

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
        rootHeader->left = IX_NO_PAGE;

        // Copy the node header to the page
        memcpy(pageData, (char*) rootHeader, sizeof(IX_NodeHeader));
        delete rootHeader;

        // Allocate a NodeValue array to the node
        IX_NodeValue* valueArray = new IX_NodeValue[degree+1];
        for (int i=0; i<=degree; i++) {
            valueArray[i] = dummyNodeValue;
        }
        valueArray[0].state = RID_FILLED;
        valueArray[0].rid = rid;
        valueArray[0].page = IX_NO_PAGE;
        int valueOffset = sizeof(IX_NodeHeader) + degree*attrLength;
        memcpy(pageData+valueOffset, (char*) valueArray, sizeof(IX_NodeValue)*(degree+1));
        delete[] valueArray;

        // Allocate a key array to the node
        char* keyData = pageData + sizeof(IX_NodeHeader);
        if (attrType == INT) {
            int* keyArray = new int[degree];
            for (int i=0; i<degree; i++) {
                keyArray[i] = -1;
            }
            int givenKey = *static_cast<int*>(pData);
            keyArray[0] = givenKey;
            memcpy(keyData, keyArray, attrLength*degree);
            delete[] keyArray;
        }
        else if (attrType == FLOAT) {
            float* keyArray = new float[degree];
            for (int i=0; i<degree; i++) {
                keyArray[i] = (float) -1;
            }
            float givenKey = *static_cast<float*>(pData);
            keyArray[0] = givenKey;
            memcpy(keyData, keyArray, attrLength*degree);
            delete[] keyArray;
        }
        else {
            char* keyArray = new char[attrLength*degree];
            for (int i=0; i<attrLength*degree; i++) {
                keyArray[i] = ' ';
            }
            char* givenKeyChar = static_cast<char*>(pData);
            strcpy(keyArray, givenKeyChar);
            memcpy(keyData, keyArray, attrLength*degree);
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
            // Check if pData is already a key
            int index = -1;
            if (attrType == INT) {
                int* keyArray = (int*) keyData;
                int givenKey = *static_cast<int*>(pData);
                for (int i=0; i<numberKeys; i++) {
                    if (keyArray[i] == givenKey) {
                        index = i;
                        break;
                    }
                }
            }
            else if (attrType == FLOAT) {
                float* keyArray = (float*) keyData;
                float givenKey = *static_cast<float*>(pData);
                for (int i=0; i<numberKeys; i++) {
                    if (keyArray[i] == givenKey) {
                        index = i;
                        break;
                    }
                }
            }
            else {
                char* keyArray = (char*) keyData;
                char* givenKeyChar = static_cast<char*>(pData);
                string givenKey(givenKeyChar);
                for (int i=0; i<numberKeys; i++) {
                    string currentKey(keyArray+i*attrLength);
                    if (currentKey == givenKey) {
                        index = i;
                        break;
                    }
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
                        bucketHeader->parentNode = rootPage;
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
                        for (int i=0; i<recordCapacity; i++) {
                            ridList[i] = dummyRID;
                        }
                        ridList[0] = rid;
                        memcpy(bucketData+sizeof(IX_BucketPageHeader), ridList, sizeof(RID)*recordCapacity);
                        delete[] ridList;

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
                    if (attrType == INT) {
                        int* keyArray = (int*) keyData;
                        int givenKey = *static_cast<int*>(pData);
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
                        memcpy(keyData, (char*) keyArray, attrLength*degree);
                    }
                    else if (attrType == FLOAT) {
                        float* keyArray = (float*) keyData;
                        float givenKey = *static_cast<float*>(pData);
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
                        memcpy(keyData, (char*) keyArray, attrLength*degree);
                    }
                    else {
                        char* keyArray = (char*) keyData;
                        char* givenKeyChar = static_cast<char*>(pData);
                        string givenKey(givenKeyChar);
                        for (int i=0; i<numberKeys; i++) {
                            string currentKey(keyArray + i*attrLength);
                            if (givenKey < currentKey) {
                                position = i;
                                break;
                            }
                        }

                        // Move the other keys forward and insert the key
                        for (int i=numberKeys; i>position; i--) {
                            for (int j=0; j<attrLength; j++) {
                                keyArray[i*attrLength + j] = keyArray[(i-1)*attrLength + j];
                            }
                            valueArray[i] = valueArray[i-1];
                        }
                        strcpy(keyArray + position*attrLength, givenKey.c_str());
                        memcpy(keyData, (char*) keyArray, attrLength*degree);
                    }

                    valueArray[position].state = RID_FILLED;
                    valueArray[position].rid = rid;
                    valueArray[position].page = IX_NO_PAGE;
                    nodeHeader->numberKeys++;

                    // Copy the keys and values to the node
                    memcpy(pageData, (char*) nodeHeader, sizeof(IX_NodeHeader));
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
                    if (attrType == INT) {
                        int* keyArray = (int*) keyData;
                        int givenKey = *static_cast<int*>(pData);
                        IX_NodeHeader* newNodeHeader = new IX_NodeHeader;
                        int* newKeyArray = new int[degree];
                        for (int i=0; i<degree; i++) {
                            newKeyArray[i] = -1;
                        }
                        IX_NodeValue* newValueArray = new IX_NodeValue[degree+1];
                        for (int i=0; i<=degree; i++) {
                            newValueArray[i] = dummyNodeValue;
                        }
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
                        for (int i=0; i<degree; i++) {
                            newRootKeyArray[i] = -1;
                        }
                        IX_NodeValue* newRootValueArray = new IX_NodeValue[degree+1];
                        for (int i=0; i<=degree; i++) {
                            newRootValueArray[i] = dummyNodeValue;
                        }
                        newRootNodeHeader->numberKeys = 1;
                        newRootNodeHeader->keyCapacity = keyCapacity;
                        newRootNodeHeader->type = ROOT;
                        newRootNodeHeader->parent = IX_NO_PAGE;
                        newRootNodeHeader->left = IX_NO_PAGE;

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
                        newNodeHeader->left = rootPage;

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

                    else if (attrType == FLOAT) {
                        float* keyArray = (float*) keyData;
                        float givenKey = *static_cast<float*>(pData);
                        IX_NodeHeader* newNodeHeader = new IX_NodeHeader;
                        float* newKeyArray = new float[degree];
                        for (int i=0; i<degree; i++) {
                            newKeyArray[i] = (float) -1;
                        }
                        IX_NodeValue* newValueArray = new IX_NodeValue[degree+1];
                        for (int i=0; i<=degree; i++) {
                            newValueArray[i] = dummyNodeValue;
                        }
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
                        for (int i=0; i<degree; i++) {
                            newRootKeyArray[i] = (float) -1;
                        }
                        IX_NodeValue* newRootValueArray = new IX_NodeValue[degree+1];
                        for (int i=0; i<=degree; i++) {
                            newRootValueArray[i] = dummyNodeValue;
                        }
                        newRootNodeHeader->numberKeys = 1;
                        newRootNodeHeader->keyCapacity = keyCapacity;
                        newRootNodeHeader->type = ROOT;
                        newRootNodeHeader->parent = IX_NO_PAGE;
                        newRootNodeHeader->left = IX_NO_PAGE;

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
                        newNodeHeader->left = rootPage;

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

                    else {
                        char* keyArray = (char*) keyData;
                        char* givenKeyChar = static_cast<char*>(pData);
                        string givenKey(givenKeyChar);
                        IX_NodeHeader* newNodeHeader = new IX_NodeHeader;
                        char* newKeyArray = new char[attrLength*degree];
                        for (int i=0; i<attrLength*degree; i++) {
                            newKeyArray[i] = ' ';
                        }
                        IX_NodeValue* newValueArray = new IX_NodeValue[degree+1];
                        for (int i=0; i<=degree; i++) {
                            newValueArray[i] = dummyNodeValue;
                        }
                        for (int i=numberKeys/2; i<numberKeys; i++) {
                            for (int j=0; j <attrLength; j++) {
                                newKeyArray[(i-numberKeys/2)*attrLength + j] = keyArray[i*attrLength + j];
                            }
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
                        string firstKey(newKeyArray);
                        if (givenKey < firstKey) {
                            // Insert in the left node
                            int position = numberKeys/2;
                            for (int i=0; i<numberKeys/2; i++) {
                                string currentKey(keyArray + i*attrLength);
                                if (givenKey < currentKey) {
                                    position = i;
                                    break;
                                }
                            }
                            for (int i=numberKeys/2; i>position; i--) {
                                for (int j=0; j<attrLength; j++) {
                                    keyArray[i*attrLength + j] = keyArray[(i-1)*attrLength + j];
                                }
                                valueArray[i] = valueArray[i-1];
                            }

                            strcpy(keyArray + position*attrLength, givenKey.c_str());
                            valueArray[position].state = RID_FILLED;
                            valueArray[position].rid = rid;
                            valueArray[position].page = IX_NO_PAGE;

                            nodeHeader->numberKeys++;
                        }
                        else {
                            // Insert in the right node
                            int position = numberKeys - numberKeys/2;
                            for (int i=0; i< numberKeys - numberKeys/2; i++) {
                                string currentKey(newKeyArray + i*attrLength);
                                if (givenKey < currentKey) {
                                    position = i;
                                    break;
                                }
                            }
                            for (int i=numberKeys-numberKeys/2; i>position; i--) {
                                for (int j=0; j<attrLength; j++) {
                                    newKeyArray[i*attrLength + j] = newKeyArray[(i-1)*attrLength + j];
                                }
                                newValueArray[i] = newValueArray[i-1];
                            }

                            strcpy(newKeyArray + position*attrLength, givenKey.c_str());
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
                        char* newRootKeyArray = new char[attrLength*degree];
                        for (int i=0; i<attrLength*degree; i++) {
                            newRootKeyArray[i] = ' ';
                        }
                        IX_NodeValue* newRootValueArray = new IX_NodeValue[degree+1];
                        for (int i=0; i<=degree; i++) {
                            newRootValueArray[i] = dummyNodeValue;
                        }
                        newRootNodeHeader->numberKeys = 1;
                        newRootNodeHeader->keyCapacity = keyCapacity;
                        newRootNodeHeader->type = ROOT;
                        newRootNodeHeader->parent = IX_NO_PAGE;
                        newRootNodeHeader->left = IX_NO_PAGE;

                        for (int i=0; i<attrLength; i++) {
                            newRootKeyArray[i] = newKeyArray[i];
                        }
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
                        newNodeHeader->left = rootPage;

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
        // TODO: HERE!
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
                        bucketHeader->parentNode = node;
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
                        for (int i=0; i<recordCapacity; i++) {
                            ridList[i] = dummyRID;
                        }
                        ridList[0] = rid;
                        memcpy(bucketData+sizeof(IX_BucketPageHeader), ridList, sizeof(RID)*recordCapacity);
                        delete[] ridList;

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
                    memcpy(nodeData, (char*) nodeHeader, sizeof(IX_NodeHeader));
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
                    for (int i=0; i<degree; i++) {
                        newKeyArray[i] = -1;
                    }
                    IX_NodeValue* newValueArray = new IX_NodeValue[degree+1];
                    for (int i=0; i<=degree; i++) {
                        newValueArray[i] = dummyNodeValue;
                    }
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

                    // Update the last pointer in the nodes
                    PageNum previousRight = valueArray[keyCapacity].page;
                    valueArray[keyCapacity].state = PAGE_ONLY;
                    valueArray[keyCapacity].page = newPageNumber;
                    valueArray[keyCapacity].rid = dummyRID;
                    newValueArray[keyCapacity].state = PAGE_ONLY;
                    newValueArray[keyCapacity].page = previousRight;
                    newValueArray[keyCapacity].rid = dummyRID;

                    // Update the right page
                    if (previousRight != -1) {
                        PF_PageHandle rightPH;
                        char* rightData;
                        if ((rc = pfFH.GetThisPage(previousRight, rightPH))) {
                            return rc;
                        }
                        if ((rc = rightPH.GetData(rightData))) {
                            return rc;
                        }
                        if ((rc = pfFH.MarkDirty(previousRight))) {
                            return rc;
                        }

                        IX_NodeHeader* rightHeader = (IX_NodeHeader*) rightData;
                        rightHeader->left = newPageNumber;
                        memcpy(rightData, rightHeader, sizeof(IX_NodeHeader));

                        if ((rc = pfFH.UnpinPage(previousRight))) {
                            return rc;
                        }
                    }

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

                    // Copy the data to the pages
                    memcpy(nodeData, (char*) nodeHeader, sizeof(IX_NodeHeader));
                    memcpy(keyData, (char*) keyArray, attrLength*degree);
                    memcpy(valueData, (char*) valueArray, sizeof(IX_NodeValue)*(degree+1));

                    memcpy(newPageData + sizeof(IX_NodeHeader), (char*) newKeyArray, attrLength*degree);
                    memcpy(newPageData + sizeof(IX_NodeHeader) + attrLength*degree, (char*) newValueArray, sizeof(IX_NodeValue)*(degree+1));
                    int keyToPushUp = newKeyArray[0];
                    delete[] newKeyArray;
                    delete[] newValueArray;

                    // Get the parent node
                    PageNum parentNode = nodeHeader->parent;
                    newNodeHeader->parent = parentNode;
                    newNodeHeader->left = node;

                    memcpy(newPageData, (char*) newNodeHeader, sizeof(IX_NodeHeader));
                    delete newNodeHeader;

                    // Call the pushKeyUp function to push the new key up
                    if ((rc = pushKeyUp((void*) &keyToPushUp, parentNode, node, newPageNumber))) {
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
                        bucketHeader->parentNode = node;
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
                        for (int i=0; i<recordCapacity; i++) {
                            ridList[i] = dummyRID;
                        }
                        ridList[0] = rid;
                        memcpy(bucketData+sizeof(IX_BucketPageHeader), ridList, sizeof(RID)*recordCapacity);
                        delete[] ridList;

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
                    memcpy(nodeData, (char*) nodeHeader, sizeof(IX_NodeHeader));
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
                    for (int i=0; i<degree; i++) {
                        newKeyArray[i] = (float) -1;
                    }
                    IX_NodeValue* newValueArray = new IX_NodeValue[degree+1];
                    for (int i=0; i<=degree; i++) {
                        newValueArray[i] = dummyNodeValue;
                    }
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

                    // Update the last pointer in the nodes
                    PageNum previousRight = valueArray[keyCapacity].page;
                    valueArray[keyCapacity].state = PAGE_ONLY;
                    valueArray[keyCapacity].page = newPageNumber;
                    valueArray[keyCapacity].rid = dummyRID;
                    newValueArray[keyCapacity].state = PAGE_ONLY;
                    newValueArray[keyCapacity].page = previousRight;
                    newValueArray[keyCapacity].rid = dummyRID;

                    // Update the right page
                    if (previousRight != -1) {
                        PF_PageHandle rightPH;
                        char* rightData;
                        if ((rc = pfFH.GetThisPage(previousRight, rightPH))) {
                            return rc;
                        }
                        if ((rc = rightPH.GetData(rightData))) {
                            return rc;
                        }
                        if ((rc = pfFH.MarkDirty(previousRight))) {
                            return rc;
                        }

                        IX_NodeHeader* rightHeader = (IX_NodeHeader*) rightData;
                        rightHeader->left = newPageNumber;
                        memcpy(rightData, rightHeader, sizeof(IX_NodeHeader));

                        if ((rc = pfFH.UnpinPage(previousRight))) {
                            return rc;
                        }
                    }

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

                    // Copy the data to the pages
                    memcpy(nodeData, (char*) nodeHeader, sizeof(IX_NodeHeader));
                    memcpy(keyData, (char*) keyArray, attrLength*degree);
                    memcpy(valueData, (char*) valueArray, sizeof(IX_NodeValue)*(degree+1));

                    memcpy(newPageData + sizeof(IX_NodeHeader), (char*) newKeyArray, attrLength*degree);
                    memcpy(newPageData + sizeof(IX_NodeHeader) + attrLength*degree, (char*) newValueArray, sizeof(IX_NodeValue)*(degree+1));
                    float keyToPushUp = newKeyArray[0];
                    delete[] newKeyArray;
                    delete[] newValueArray;


                    // Get the parent node
                    PageNum parentNode = nodeHeader->parent;
                    newNodeHeader->parent = parentNode;
                    newNodeHeader->left = node;

                    memcpy(newPageData, (char*) newNodeHeader, sizeof(IX_NodeHeader));
                    delete newNodeHeader;

                    // Call the pushKeyUp function to push the new key up
                    if ((rc = pushKeyUp((void*) &keyToPushUp, parentNode, node, newPageNumber))) {
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
            char* keyArray = (char*) keyData;
            char* givenKeyChar = static_cast<char*>(pData);
            string givenKey(givenKeyChar);
            int index = -1;
            for (int i=0; i<numberKeys; i++) {
                string currentKey(keyArray + i*attrLength);
                if (currentKey == givenKey) {
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
                        bucketHeader->parentNode = node;
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
                        for (int i=0; i<recordCapacity; i++) {
                            ridList[i] = dummyRID;
                        }
                        ridList[0] = rid;
                        memcpy(bucketData+sizeof(IX_BucketPageHeader), ridList, sizeof(RID)*recordCapacity);
                        delete[] ridList;

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
                        string currentKey(keyArray + i*attrLength);
                        if (givenKey < currentKey) {
                            position = i;
                            break;
                        }
                    }

                    // Move the other keys forward and insert the key
                    for (int i=numberKeys; i>position; i--) {
                        for (int j=0; j<attrLength; j++) {
                            keyArray[i*attrLength + j] = keyArray[(i-1)*attrLength + j];
                        }
                        valueArray[i] = valueArray[i-1];
                    }
                    strcpy(keyArray + position*attrLength, givenKey.c_str());
                    valueArray[position].state = RID_FILLED;
                    valueArray[position].rid = rid;
                    valueArray[position].page = IX_NO_PAGE;

                    nodeHeader->numberKeys++;

                    // Copy the keys and values to the node
                    memcpy(nodeData, (char*) nodeHeader, sizeof(IX_NodeHeader));
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
                    char* newKeyArray = new char[attrLength*degree];
                    for (int i=0; i<attrLength*degree; i++) {
                        newKeyArray[i] = ' ';
                    }
                    IX_NodeValue* newValueArray = new IX_NodeValue[degree+1];
                    for (int i=0; i<=degree; i++) {
                        newValueArray[i] = dummyNodeValue;
                    }
                    for (int i=numberKeys/2; i<numberKeys; i++) {
                        for (int j=0; j<attrLength; j++) {
                            newKeyArray[(i-numberKeys/2)*attrLength + j] = keyArray[i*attrLength + j];
                        }
                        newValueArray[i-numberKeys/2] = valueArray[i];
                    }

                    // Update the node headers
                    nodeHeader->numberKeys = numberKeys/2;
                    nodeHeader->type = LEAF;
                    newNodeHeader->numberKeys = numberKeys - numberKeys/2;
                    newNodeHeader->keyCapacity = keyCapacity;
                    newNodeHeader->type = LEAF;

                    // Update the last pointer in the nodes
                    PageNum previousRight = valueArray[keyCapacity].page;
                    valueArray[keyCapacity].state = PAGE_ONLY;
                    valueArray[keyCapacity].page = newPageNumber;
                    valueArray[keyCapacity].rid = dummyRID;
                    newValueArray[keyCapacity].state = PAGE_ONLY;
                    newValueArray[keyCapacity].page = previousRight;
                    newValueArray[keyCapacity].rid = dummyRID;

                    // Update the right page
                    if (previousRight != -1) {
                        PF_PageHandle rightPH;
                        char* rightData;
                        if ((rc = pfFH.GetThisPage(previousRight, rightPH))) {
                            return rc;
                        }
                        if ((rc = rightPH.GetData(rightData))) {
                            return rc;
                        }
                        if ((rc = pfFH.MarkDirty(previousRight))) {
                            return rc;
                        }

                        IX_NodeHeader* rightHeader = (IX_NodeHeader*) rightData;
                        rightHeader->left = newPageNumber;
                        memcpy(rightData, rightHeader, sizeof(IX_NodeHeader));

                        if ((rc = pfFH.UnpinPage(previousRight))) {
                            return rc;
                        }
                    }

                    // Insert the new key
                    string firstKey(newKeyArray);
                    if (givenKey < firstKey) {
                        // Insert in the left node
                        int position = numberKeys/2;
                        for (int i=0; i<numberKeys/2; i++) {
                            string currentKey(keyArray + i*attrLength);
                            if (givenKey < currentKey) {
                                position = i;
                                break;
                            }
                        }
                        for (int i=numberKeys/2; i>position; i--) {
                            for (int j=0; j<attrLength; j++) {
                                keyArray[i*attrLength + j] = keyArray[(i-1)*attrLength + j];
                            }
                            valueArray[i] = valueArray[i-1];
                        }
                        strcpy(keyArray + position*attrLength, givenKey.c_str());
                        valueArray[position].state = RID_FILLED;
                        valueArray[position].rid = rid;
                        valueArray[position].page = IX_NO_PAGE;

                        nodeHeader->numberKeys++;
                    }
                    else {
                        // Insert in the right node
                        int position = numberKeys - numberKeys/2;
                        for (int i=0; i< numberKeys - numberKeys/2; i++) {
                            string currentKey(newKeyArray + i*attrLength);
                            if (givenKey < currentKey) {
                                position = i;
                                break;
                            }
                        }
                        for (int i=numberKeys-numberKeys/2; i>position; i--) {
                            for (int j=0; j<attrLength; j++) {
                                newKeyArray[i*attrLength + j] = newKeyArray[(i-1)*attrLength + j];
                            }
                            newValueArray[i] = newValueArray[i-1];
                        }
                        strcpy(newKeyArray + position*attrLength, givenKey.c_str());
                        newValueArray[position].state = RID_FILLED;
                        newValueArray[position].rid = rid;
                        newValueArray[position].page = IX_NO_PAGE;

                        newNodeHeader->numberKeys++;
                    }

                    // Copy the data to the pages
                    memcpy(nodeData, (char*) nodeHeader, sizeof(IX_NodeHeader));
                    memcpy(keyData, (char*) keyArray, attrLength*degree);
                    memcpy(valueData, (char*) valueArray, sizeof(IX_NodeValue)*(degree+1));

                    memcpy(newPageData + sizeof(IX_NodeHeader), (char*) newKeyArray, attrLength*degree);
                    memcpy(newPageData + sizeof(IX_NodeHeader) + attrLength*degree, (char*) newValueArray, sizeof(IX_NodeValue)*(degree+1));
                    delete[] newValueArray;


                    // Get the parent node
                    PageNum parentNode = nodeHeader->parent;
                    newNodeHeader->parent = parentNode;
                    newNodeHeader->left = node;

                    memcpy(newPageData, (char*) newNodeHeader, sizeof(IX_NodeHeader));
                    delete newNodeHeader;

                    // Call the pushKeyUp function to push the new key up
                    if ((rc = pushKeyUp(newKeyArray, parentNode, node, newPageNumber))) {
                        return rc;
                    }
                    delete[] newKeyArray;

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
            char* keyArray = (char*) keyData;
            char* givenKeyChar = static_cast<char*>(pData);
            string givenKey(givenKeyChar);

            // Search for corresponding pointer to next node
            int position = numberKeys;
            for (int i=0; i<numberKeys; i++) {
                string currentKey(keyArray + i*attrLength);
                if (givenKey < currentKey) {
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

// Method: pushKeyUp(void* pData, PageNum node, PageNum left, PageNum right)
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
                valueArray[i+1] = valueArray[i];
            }
            valueArray[position+1] = valueArray[position];
            keyArray[position] = givenKey;
            valueArray[position].state = PAGE_ONLY;
            valueArray[position].page = left;
            valueArray[position].rid = dummyRID;
            valueArray[position+1].page = right;

            nodeHeader->numberKeys++;

            // Copy the keys and values to the node
            memcpy(nodeData, (char*) nodeHeader, sizeof(IX_NodeHeader));
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
            for (int i=0; i<degree; i++) {
                newKeyArray[i] = -1;
            }
            IX_NodeValue* newValueArray = new IX_NodeValue[degree+1];
            for (int i=0; i<=degree; i++) {
                newValueArray[i] = dummyNodeValue;
            }
            for (int i=numberKeys/2; i<numberKeys; i++) {
                newKeyArray[i-numberKeys/2] = keyArray[i];
                newValueArray[i-numberKeys/2] = valueArray[i];
            }
            newValueArray[numberKeys-numberKeys/2] = valueArray[numberKeys];

            // Update the node headers
            nodeHeader->numberKeys = numberKeys/2;
            nodeHeader->type = NODE;
            newNodeHeader->numberKeys = numberKeys - numberKeys/2;
            newNodeHeader->keyCapacity = keyCapacity;
            newNodeHeader->type = NODE;

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
                    valueArray[i+1] = valueArray[i];
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
                    newValueArray[i+1] = newValueArray[i];
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
                for (int i=0; i<degree; i++) {
                    newRootKeyArray[i] = -1;
                }
                IX_NodeValue* newRootValueArray = new IX_NodeValue[degree+1];
                for (int i=0; i<=degree; i++) {
                    newRootValueArray[i] = dummyNodeValue;
                }
                newRootNodeHeader->numberKeys = 1;
                newRootNodeHeader->keyCapacity = keyCapacity;
                newRootNodeHeader->type = ROOT;
                newRootNodeHeader->parent = IX_NO_PAGE;
                newRootNodeHeader->left = IX_NO_PAGE;

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
                newNodeHeader->left = node;

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
                newNodeHeader->left = node;
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
                valueArray[i+1] = valueArray[i];
            }
            valueArray[position+1] = valueArray[position];
            keyArray[position] = givenKey;
            valueArray[position].state = PAGE_ONLY;
            valueArray[position].page = left;
            valueArray[position].rid = dummyRID;
            valueArray[position+1].page = right;

            nodeHeader->numberKeys++;

            // Copy the keys and values to the node
            memcpy(nodeData, (char*) nodeHeader, sizeof(IX_NodeHeader));
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
            for (int i=0; i<degree; i++) {
                newKeyArray[i] = (float) -1;
            }
            IX_NodeValue* newValueArray = new IX_NodeValue[degree+1];
            for (int i=0; i<=degree; i++) {
                newValueArray[i] = dummyNodeValue;
            }
            for (int i=numberKeys/2; i<numberKeys; i++) {
                newKeyArray[i-numberKeys/2] = keyArray[i];
                newValueArray[i-numberKeys/2] = valueArray[i];
            }
            newValueArray[numberKeys-numberKeys/2] = valueArray[numberKeys];

            // Update the node headers
            nodeHeader->numberKeys = numberKeys/2;
            nodeHeader->type = NODE;
            newNodeHeader->numberKeys = numberKeys - numberKeys/2;
            newNodeHeader->keyCapacity = keyCapacity;
            newNodeHeader->type = NODE;

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
                    valueArray[i+1] = valueArray[i];
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
                    newValueArray[i+1] = newValueArray[i];
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
                for (int i=0; i<degree; i++) {
                    newRootKeyArray[i] = (float) -1;
                }
                IX_NodeValue* newRootValueArray = new IX_NodeValue[degree+1];
                for (int i=0; i<=degree; i++) {
                    newRootValueArray[i] = dummyNodeValue;
                }
                newRootNodeHeader->numberKeys = 1;
                newRootNodeHeader->keyCapacity = keyCapacity;
                newRootNodeHeader->type = ROOT;
                newRootNodeHeader->parent = IX_NO_PAGE;
                newRootNodeHeader->left = IX_NO_PAGE;

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
                newNodeHeader->left = node;

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
                newNodeHeader->left = node;
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
        char* keyArray = (char*) keyData;
        char* givenKeyChar = static_cast<char*>(pData);
        string givenKey(givenKeyChar);

        // If the node is not full
        if (numberKeys < keyCapacity) {
            // Find the position for this key
            int position = numberKeys;
            for (int i=0; i<numberKeys; i++) {
                string currentKey(keyArray + i*attrLength);
                if (givenKey < currentKey) {
                    position = i;
                    break;
                }
            }

            // Move the other keys forward and insert the key
            for (int i=numberKeys; i>position; i--) {
                for (int j=0; j<attrLength; j++) {
                    keyArray[i*attrLength + j] = keyArray[(i-1)*attrLength + j];
                }
                valueArray[i+1] = valueArray[i];
            }
            valueArray[position+1] = valueArray[position];
            strcpy(keyArray + position*attrLength, givenKey.c_str());
            valueArray[position].state = PAGE_ONLY;
            valueArray[position].page = left;
            valueArray[position].rid = dummyRID;
            valueArray[position+1].page = right;

            nodeHeader->numberKeys++;

            // Copy the keys and values to the node
            memcpy(nodeData, (char*) nodeHeader, sizeof(IX_NodeHeader));
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
            char* newKeyArray = new char[attrLength*degree];
            for (int i=0; i<attrLength*degree; i++) {
                newKeyArray[i] = ' ';
            }
            IX_NodeValue* newValueArray = new IX_NodeValue[degree+1];
            for (int i=0; i<=degree; i++) {
                newValueArray[i] = dummyNodeValue;
            }
            for (int i=numberKeys/2; i<numberKeys; i++) {
                for (int j=0; j<attrLength; j++) {
                    newKeyArray[(i-numberKeys/2)*attrLength + j] = keyArray[i*attrLength + j];
                }
                newValueArray[i-numberKeys/2] = valueArray[i];
            }
            newValueArray[numberKeys-numberKeys/2] = valueArray[numberKeys];

            // Update the node headers
            nodeHeader->numberKeys = numberKeys/2;
            nodeHeader->type = NODE;
            newNodeHeader->numberKeys = numberKeys - numberKeys/2;
            newNodeHeader->keyCapacity = keyCapacity;
            newNodeHeader->type = NODE;

            // Insert the new key
            string firstKey(newKeyArray);
            if (givenKey < firstKey) {
                // Insert in the left node
                int position = numberKeys/2;
                for (int i=0; i<numberKeys/2; i++) {
                    string currentKey(keyArray + i*attrLength);
                    if (givenKey < currentKey) {
                        position = i;
                        break;
                    }
                }
                for (int i=numberKeys/2; i>position; i--) {
                    for (int j=0; j<attrLength; j++) {
                        keyArray[i*attrLength + j] = keyArray[(i-1)*attrLength + j];
                    }
                    valueArray[i+1] = valueArray[i];
                }
                strcpy(keyArray + position*attrLength, givenKey.c_str());
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
                    string currentKey(newKeyArray + i*attrLength);
                    if (givenKey < currentKey) {
                        position = i;
                        break;
                    }
                }
                for (int i=newNodeHeader->numberKeys; i>position; i--) {
                    for (int j=0; j<attrLength; j++) {
                        newKeyArray[i*attrLength + j] = newKeyArray[(i-1)*attrLength + j];
                    }
                    newValueArray[i+1] = newValueArray[i];
                }
                strcpy(newKeyArray + position*attrLength, givenKey.c_str());
                newValueArray[position].state = PAGE_ONLY;
                newValueArray[position].page = left;
                newValueArray[position].rid = dummyRID;
                newValueArray[position+1].page = right;

                newNodeHeader->numberKeys++;
            }

            // Remove the first key from the right node
            string keyToPushUp(newKeyArray);
            for (int i=0; i<newNodeHeader->numberKeys; i++) {
                for (int j=0; j<attrLength; j++) {
                    newKeyArray[i*attrLength + j] = newKeyArray[(i+1)*attrLength + j];
                }
                newValueArray[i] = newValueArray[i+1];
            }
            newNodeHeader->numberKeys--;

            // Update the parent pointers of the children
            PageNum childPage;
            PF_PageHandle childPFPH;
            char* childData;
            for (int i=0; i<=newNodeHeader->numberKeys; i++) {
                childPage = newValueArray[i].page;
                if (childPage != IX_NO_PAGE) {
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
                char* newRootKeyArray = new char[attrLength*degree];
                for (int i=0; i<attrLength*degree; i++) {
                    newRootKeyArray[i] = ' ';
                }
                IX_NodeValue* newRootValueArray = new IX_NodeValue[degree+1];
                for (int i=0; i<=degree; i++) {
                    newRootValueArray[i] = dummyNodeValue;
                }
                newRootNodeHeader->numberKeys = 1;
                newRootNodeHeader->keyCapacity = keyCapacity;
                newRootNodeHeader->type = ROOT;
                newRootNodeHeader->parent = IX_NO_PAGE;
                newRootNodeHeader->left = IX_NO_PAGE;

                strcpy(newRootKeyArray, keyToPushUp.c_str());
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
                newNodeHeader->left = node;

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
                newNodeHeader->left = node;
                if ((rc = pushKeyUp(newKeyArray, parentNode, node, newPageNumber))) {
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


/************** CODE FOR DELETE ENTRY *****************/

// Method: DeleteEntry(void *pData, const RID &rid)
// Delete a new index entry
/* Steps:
    1) Get the root page
    2) If root page does not exist, return warning
    3) Else get the data from the root page
    4) If type of node is ROOT_LEAF
        - Call DeleleFromLeaf with the root node
    5) Else if type is ROOT
        - Get the leaf page containing the data recursively
        - Call DeleteFromLeaf with the leaf page
    6) Unpin the root page
    7) Return OK
*/
RC IX_IndexHandle::DeleteEntry(void *pData, const RID &rid) {
    // Declare an integer for the return code
    int rc;

    // Check if the index handle is open
    if (!isOpen) {
        return IX_INDEX_CLOSED;
    }

    // Check if pData is null
    if (pData == NULL) {
        return IX_NULL_ENTRY;
    }

    // Get the root page
    PageNum rootPage = indexHeader.rootPage;
    if (rootPage == IX_NO_PAGE) {
        return IX_DELETE_ENTRY_NOT_FOUND;
    }
    else {
        // Get the root page data
        PF_PageHandle pfPH;
        char* rootData;
        if ((rc = pfFH.GetThisPage(rootPage, pfPH))) {
            return rc;
        }
        if ((rc = pfPH.GetData(rootData))) {
            return rc;
        }

        IX_NodeHeader* rootHeader = (IX_NodeHeader*) rootData;
        IX_NodeType type = rootHeader->type;

        // If the type is ROOT_LEAF
        if (type == ROOT_LEAF) {
            if ((rc = DeleteFromLeaf(pData, rid, rootPage))) {
                return rc;
            }
        }

        // Else if the type is ROOT
        else {
            PageNum leafPage = IX_NO_PAGE;
            if ((rc = SearchEntry(pData, rootPage, leafPage))) {
                return rc;
            }
            if (leafPage == IX_NO_PAGE) {
                return IX_DELETE_ENTRY_NOT_FOUND;
            }
            else {
                if ((rc = DeleteFromLeaf(pData, rid, leafPage))) {
                    return rc;
                }
            }
        }

        // Store the last deleted entry
        if (lastDeletedEntry.keyValue == NULL) {
            lastDeletedEntry.keyValue = new char[indexHeader.attrLength];
        }
        memcpy(lastDeletedEntry.keyValue, pData, indexHeader.attrLength);
        lastDeletedEntry.rid = rid;

        // Unpin the root page
        if ((rc = pfFH.UnpinPage(rootPage))) {
            return rc;
        }

        // Return OK
        return OK_RC;
    }
}

// Method: SearchEntry(void* pData, PageNum node, PageNum &pageNumber)
// Recursively search for an index entry
/* Steps:
    1) Get the data in the node page
    2) Check the type of node
    3) If leaf node
        - Set page number
        - Unpin the current node page
    4) If root/internal node
        - Search for corresponding pointer to next node
        - Unpin the current node page
        - Recursive call with the next node page
    5) Return OK
*/
RC IX_IndexHandle::SearchEntry(void* pData, PageNum node, PageNum &pageNumber) {
    // Declare an integer for the return code
    int rc;

    if (node == IX_NO_PAGE) {
        return IX_DELETE_ENTRY_NOT_FOUND;
    }

    // Get the data in the node page
    PF_PageHandle pfPH;
    if ((rc = pfFH.GetThisPage(node, pfPH))) {
        return rc;
    }
    char* nodeData;
    if ((rc = pfPH.GetData(nodeData))) {
        return rc;
    }

    AttrType attrType = indexHeader.attrType;
    int attrLength = indexHeader.attrLength;
    int degree = indexHeader.degree;

    // Get the node type
    IX_NodeHeader* nodeHeader = (IX_NodeHeader*) nodeData;
    IX_NodeType nodeType = nodeHeader->type;
    int numberKeys = nodeHeader->numberKeys;
    char* keyData = nodeData + sizeof(IX_NodeHeader);
    char* valueData = keyData + attrLength*degree;
    IX_NodeValue* valueArray = (IX_NodeValue*) valueData;

    // If leaf node
    if (nodeType == LEAF || nodeType == ROOT_LEAF) {
        // Set the page number to return
        pageNumber = node;

        // Unpin the node page
        if ((rc = pfFH.UnpinPage(node))) {
            return rc;
        }
    }

    // Else if it is an internal node find the next page
    else if (nodeType == NODE || nodeType == ROOT) {
        PageNum nextPage = IX_NO_PAGE;
        if (attrType == INT) {
            int* keyArray = (int*) keyData;
            int intValue = *static_cast<int*>(pData);
            if (intValue < keyArray[0]) {
                nextPage = valueArray[0].page;
            }
            else if (intValue >= keyArray[numberKeys-1]) {
                nextPage = valueArray[numberKeys].page;
            }
            else {
                bool found;
                for (int i=1; i<numberKeys; i++) {
                    if (satisfiesInterval(keyArray[i-1], keyArray[i], intValue)) {
                        nextPage = valueArray[i].page;
                        found = true;
                        break;
                    }
                }
                if (!found) nextPage = valueArray[numberKeys].page;
            }
        }
        else if (attrType == FLOAT) {
            float* keyArray = (float*) keyData;
            float floatValue = *static_cast<float*>(pData);
            if (floatValue < keyArray[0]) {
                nextPage = valueArray[0].page;
            }
            else if (floatValue >= keyArray[numberKeys-1]) {
                nextPage = valueArray[numberKeys].page;
            }
            else {
                bool found;
                for (int i=1; i<numberKeys; i++) {
                    if (satisfiesInterval(keyArray[i-1], keyArray[i], floatValue)) {
                        nextPage = valueArray[i].page;
                        found = true;
                        break;
                    }
                }
                if (!found) nextPage = valueArray[numberKeys].page;
            }
        }
        else {
            char* keyArray = (char*) keyData;
            char* valueChar = static_cast<char*>(pData);
            string stringValue(valueChar);
            string firstKey(keyArray);
            string lastKey(keyArray + (numberKeys-1)*attrLength);
            if (stringValue < firstKey) {
                nextPage = valueArray[0].page;
            }
            else if (stringValue >= lastKey) {
                nextPage = valueArray[numberKeys].page;
            }
            else {
                bool found;
                for (int i=1; i<numberKeys; i++) {
                    string currentKey(keyArray + i*attrLength);
                    string previousKey(keyArray + (i-1)*attrLength);
                    if (satisfiesInterval(previousKey, currentKey, stringValue)) {
                        nextPage = valueArray[i].page;
                        found = true;
                        break;
                    }
                }
                if (!found) nextPage = valueArray[numberKeys].page;
            }
        }

        // Unpin the current page
        if ((rc = pfFH.UnpinPage(node))) {
            return rc;
        }

        // Recursive call with the next node
        if ((rc = SearchEntry(pData, nextPage, pageNumber))) {
            return rc;
        }
    }

    // Return OK
    return OK_RC;
}

// Method: DeleteFromLeaf(void* pData, const RID &rid, PageNum node)
// Delete an entry from a leaf node
/* Steps:
    1) Get the page data
    2) Search for the key in the node
    3) If not found, return warning
    4) Get RID at the corresponding location
    5) If RID match
        - If bucket exists
            - Replace RID with last RID from bucket
            - Unpin bucket page
            - If bucket becomes empty, dispose the bucket page and change node entry
        - If bucket does not exist
            - Delete entry from the node
            - If node becomes empty, dispose page and push the deletion up
            - Else if first entry in node, replace the key in the parent node
    6) If RID does not match
        - If bucket exists
            - Search for the RID in the ridList
            - If not found, return warning
            - If found, shift all entries to the left
            - Unpin bucket page
            - If bucket becomes empty, dispose the bucket page and change node entry
        - If bucket does not exist
            - Return warning
    7) Unpin node page
    8) Return OK
*/
RC IX_IndexHandle::DeleteFromLeaf(void* pData, const RID &rid, PageNum node) {
    // Declare an integer for the return code
    int rc;

    bool disposeFlag = false;
    // Get the node data
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
    int numberKeys = nodeHeader->numberKeys;
    char* keyData = nodeData + sizeof(IX_NodeHeader);
    char* valueData = keyData + attrLength*degree;
    IX_NodeValue* valueArray = (IX_NodeValue*) valueData;
    int keyPosition = -1;

    if (attrType == INT) {
        int* keyArray = (int*) keyData;
        int givenValue = *static_cast<int*>(pData);
        for (int i=0; i<numberKeys; i++) {
            if (keyArray[i] == givenValue) {
                keyPosition = i;
                break;
            }
        }
    }
    else if (attrType == FLOAT) {
        float* keyArray = (float*) keyData;
        float givenValue = *static_cast<float*>(pData);
        for (int i=0; i<numberKeys; i++) {
            if (keyArray[i] == givenValue) {
                keyPosition = i;
                break;
            }
        }
    }
    else {
        char* keyArray = (char*) keyData;
        char* givenValueChar = static_cast<char*>(pData);
        string givenValue(givenValueChar);
        for (int i=0; i<numberKeys; i++) {
            string currentKey(keyArray + i*attrLength);
            if (currentKey == givenValue) {
                keyPosition = i;
                break;
            }
        }
    }

    // If not found
    if (keyPosition == -1) {
        return IX_DELETE_ENTRY_NOT_FOUND;
    }
    else {
        // Check if the RID at the position matches
        IX_NodeValue value = valueArray[keyPosition];
        PageNum bucketPage = value.page;

        PageNum p;
        SlotNum s;
        if ((rc = value.rid.GetPageNum(p)) || (rc = value.rid.GetSlotNum(s))) return rc;

        if (compareRIDs(rid, value.rid)) {
            // If bucket exists
            if (bucketPage != IX_NO_PAGE) {
                // Get the bucket data
                PF_PageHandle bucketPH;
                char* bucketData;
                if ((rc = pfFH.GetThisPage(bucketPage, bucketPH))) {
                    return rc;
                }
                if ((rc = bucketPH.GetData(bucketData))) {
                    return rc;
                }
                if ((rc = pfFH.MarkDirty(bucketPage))) {
                    return rc;
                }

                IX_BucketPageHeader* bucketHeader = (IX_BucketPageHeader*) bucketData;
                int numberRecords = bucketHeader->numberRecords;
                char* ridData = bucketData + sizeof(IX_BucketPageHeader);
                RID* ridList = (RID*) ridData;

                // Get the last RID from the bucket
                RID newRID = ridList[numberRecords-1];
                valueArray[keyPosition].rid = newRID;
                bucketHeader->numberRecords--;
                memcpy(bucketData, (char*) bucketHeader, sizeof(IX_BucketPageHeader));

                // Unpin bucket page
                if ((rc = pfFH.UnpinPage(bucketPage))) {
                    return rc;
                }

                // Dispose bucket page if empty
                if(bucketHeader->numberRecords == 0) {
                    valueArray[keyPosition].page = IX_NO_PAGE;
                    if ((rc = pfFH.DisposePage(bucketPage))) {
                        return rc;
                    }
                }
            }

            // Else if bucket does not exist
            else {
                if ((rc = pfFH.MarkDirty(node))) {
                    return rc;
                }
                // Shift the keys and values to the left
                for (int i=keyPosition+1; i<numberKeys; i++) {
                    if (attrType == INT) {
                        int* keyArray = (int*) keyData;
                        keyArray[i-1] = keyArray[i];
                        memcpy(keyData, (char*) keyArray, attrLength*degree);
                    }
                    else if (attrType == FLOAT) {
                        float* keyArray = (float*) keyData;
                        keyArray[i-1] = keyArray[i];
                        memcpy(keyData, (char*) keyArray, attrLength*degree);
                    }
                    else {
                        char* keyArray = (char*) keyData;
                        for (int j=0; j<attrLength; j++) {
                            keyArray[(i-1)*attrLength + j] = keyArray[i*attrLength + j];
                        }
                        memcpy(keyData, (char*) keyArray, attrLength*degree);
                    }
                    valueArray[i-1] = valueArray[i];
                }
                nodeHeader->numberKeys--;

                // Check if the node has become empty
                if (nodeHeader->numberKeys == 0) {
                    disposeFlag = true;

                    // Change the pointer from the left page
                    PageNum right = valueArray[degree].page;
                    PageNum left = nodeHeader->left;
                    if (left != IX_NO_PAGE) {
                        PF_PageHandle leftPH;
                        char* leftData;
                        if ((rc = pfFH.GetThisPage(left, leftPH))) {
                            return rc;
                        }
                        if ((rc = leftPH.GetData(leftData))) {
                            return rc;
                        }
                        if ((rc = pfFH.MarkDirty(left))) {
                            return rc;
                        }

                        char* leftValueData = leftData + sizeof(IX_NodeHeader) + attrLength*degree;
                        IX_NodeValue* leftValueArray = (IX_NodeValue*) leftValueData;
                        leftValueArray[degree].page = right;
                        memcpy(leftValueData, (char*) leftValueArray, sizeof(IX_NodeValue)*(degree+1));

                        if ((rc = pfFH.UnpinPage(left))) {
                            return rc;
                        }
                    }

                    // Change the pointer in the right page
                    if (right != IX_NO_PAGE) {
                        PF_PageHandle rightPH;
                        char* rightData;
                        if ((rc = pfFH.GetThisPage(right, rightPH))) {
                            return rc;
                        }
                        if ((rc = rightPH.GetData(rightData))) {
                            return rc;
                        }
                        if ((rc = pfFH.MarkDirty(right))) {
                            return rc;
                        }

                        IX_NodeHeader* rightHeader = (IX_NodeHeader*) rightData;
                        rightHeader->left = left;
                        memcpy(rightData, (char*) rightHeader, sizeof(IX_NodeHeader));

                        if ((rc = pfFH.UnpinPage(right))) {
                            return rc;
                        }
                    }

                    // Push the node deletion to the parent
                    PageNum parent = nodeHeader->parent;
                    if (parent == IX_NO_PAGE) {
                        indexHeader.rootPage = IX_NO_PAGE;
                        headerModified = TRUE;
                    }
                    else {
                        // Call the push deletion up method
                        if ((rc = pushDeletionUp(parent, node))) {
                            return rc;
                        }
                    }
                }
            }
        }
        else {
            // If bucket exists
            if (bucketPage != IX_NO_PAGE) {
                // Get the bucket data
                PF_PageHandle bucketPH;
                char* bucketData;
                if ((rc = pfFH.GetThisPage(bucketPage, bucketPH))) {
                    return rc;
                }
                if ((rc = bucketPH.GetData(bucketData))) {
                    return rc;
                }
                if ((rc = pfFH.MarkDirty(bucketPage))) {
                    return rc;
                }

                IX_BucketPageHeader* bucketHeader = (IX_BucketPageHeader*) bucketData;
                int numberRecords = bucketHeader->numberRecords;
                char* ridData = bucketData + sizeof(IX_BucketPageHeader);
                RID* ridList = (RID*) ridData;

                // Search for the RID
                int position = -1;
                for (int i=0; i<numberRecords; i++) {
                    if (compareRIDs(ridList[i], rid)) {
                        position = i;
                        break;
                    }
                }
                if (position == -1) {
                    return IX_DELETE_ENTRY_NOT_FOUND;
                }

                // Shift the RIDs to the left
                for (int i=position+1; i<numberRecords; i++) {
                    ridList[i-1] = ridList[i];
                }
                bucketHeader->numberRecords--;
                memcpy(bucketData, (char*) bucketHeader, sizeof(IX_BucketPageHeader));
                memcpy(ridData, (char*) ridList, sizeof(RID)*bucketHeader->numberRecords);

                // Unpin bucket page
                if ((rc = pfFH.UnpinPage(bucketPage))) {
                    return rc;
                }

                // Dispose bucket page if empty
                if(bucketHeader->numberRecords == 0) {
                    valueArray[keyPosition].page = IX_NO_PAGE;
                    if ((rc = pfFH.DisposePage(bucketPage))) {
                        return rc;
                    }
                }
            }

            // Else if bucket does not exist
            else {
                return IX_DELETE_ENTRY_NOT_FOUND;
            }
        }

        // Copy the data to the node page
        memcpy(nodeData, (char*) nodeHeader, sizeof(IX_NodeHeader));
        memcpy(valueData, (char*) valueArray, sizeof(IX_NodeValue)*(degree+1));

        // Unpin the node page
        if ((rc = pfFH.UnpinPage(node))) {
            return rc;
        }

        // Dispose the node page if dispose flag is true
        if (disposeFlag) {
            if ((rc = pfFH.DisposePage(node))) {
                return rc;
            }
        }
    }

    // Return OK
    return OK_RC;
}

// Method: pushDeletionUp(PageNum node, PageNum child)
// Recursively push the deletion up the tree
/* Steps:
    1) Get the node data
    2) Find the position of the key to delete
    3) Shift the keys and values to the left
    4) If the node is ROOT
        - If the node has become empty, set root page to IX_NO_PAGE
    5) Else if the node is not ROOT
        - If the node has become empty, dispose the page
        - Call recursive function with parent node
    6) Unpin and dispose the page if needed
    7) Return OK
*/
RC IX_IndexHandle::pushDeletionUp(PageNum node, PageNum child) {
    // Declare an integer for the return code
    int rc;
    bool disposeFlag = false;

    if (node == IX_NO_PAGE) {
        return IX_INCONSISTENT_NODE;
    }

    // Get the node data
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

    int attrLength = indexHeader.attrLength;
    AttrType attrType = indexHeader.attrType;
    int degree = indexHeader.degree;
    IX_NodeHeader* nodeHeader = (IX_NodeHeader*) nodeData;
    char* keyData = nodeData + sizeof(IX_NodeHeader);
    char* valueData = keyData + attrLength*degree;
    IX_NodeValue* valueArray = (IX_NodeValue*) valueData;
    int numberKeys = nodeHeader->numberKeys;
    IX_NodeType type = nodeHeader->type;


    // Find the position of the key to delete
    int keyPosition = -1;
    for (int i=0; i<=numberKeys; i++) {
        if (valueArray[i].page == child) {
            keyPosition = i;
        }
    }

    // If the number of keys is 1
    if (numberKeys == 1) {
        if (keyPosition == -1) {
            return IX_INCONSISTENT_NODE;
        }
        else {
            valueArray[keyPosition].page = IX_NO_PAGE;
        }

        memcpy(valueData, (char*) valueArray, sizeof(IX_NodeValue)*(degree+1));
    }

    // Else if more than 1 key
    else {
        // Shift the keys and values to the left
        if (keyPosition == -1) {
            return IX_INCONSISTENT_NODE;
        }
        else if (keyPosition == 0) {
            if (attrType == INT) {
                int* keyArray = (int*) keyData;
                for (int i=1; i<numberKeys; i++) {
                    keyArray[i-1] = keyArray[i];
                    valueArray[i-1] = valueArray[i];
                }
                valueArray[numberKeys-1] = valueArray[numberKeys];
                memcpy(keyData, (char*) keyArray, attrLength*degree);
            }
            else if (attrType == FLOAT) {
                float* keyArray = (float*) keyData;
                for (int i=1; i<numberKeys; i++) {
                    keyArray[i-1] = keyArray[i];
                    valueArray[i-1] = valueArray[i];
                }
                valueArray[numberKeys-1] = valueArray[numberKeys];
                memcpy(keyData, (char*) keyArray, attrLength*degree);
            }
            else {
                char* keyArray = (char*) keyData;
                for (int i=1; i<numberKeys; i++) {
                    for (int j=0; j<attrLength; j++) {
                        keyArray[(i-1)*attrLength + j] = keyArray[i*attrLength + j];
                    }
                    valueArray[i-1] = valueArray[i];
                }
                valueArray[numberKeys-1] = valueArray[numberKeys];
                memcpy(keyData, (char*) keyArray, attrLength*degree);
            }
        }
        else {
            if (attrType == INT) {
                int* keyArray = (int*) keyData;
                for (int i=keyPosition; i<numberKeys; i++) {
                    keyArray[i-1] = keyArray[i];
                    valueArray[i] = valueArray[i+1];
                }
                // valueArray[numberKeys-1] = valueArray[numberKeys];
                memcpy(keyData, (char*) keyArray, attrLength*degree);
            }
            else if (attrType == FLOAT) {
                float* keyArray = (float*) keyData;
                for (int i=keyPosition; i<numberKeys; i++) {
                    keyArray[i-1] = keyArray[i];
                    valueArray[i] = valueArray[i+1];
                }
                // valueArray[numberKeys-1] = valueArray[numberKeys];
                memcpy(keyData, (char*) keyArray, attrLength*degree);
            }
            else {
                char* keyArray = (char*) keyData;
                for (int i=keyPosition; i<numberKeys; i++) {
                    for (int j=0; j<attrLength; j++) {
                        keyArray[(i-1)*attrLength + j] = keyArray[i*attrLength + j];
                    }
                    valueArray[i] = valueArray[i+1];
                }
                // valueArray[numberKeys-1] = valueArray[numberKeys];
                memcpy(keyData, (char*) keyArray, attrLength*degree);
            }
        }

        // Update the number of keys
        nodeHeader->numberKeys--;
        for (int i=0; i<nodeHeader->numberKeys; i++) {

        }

        // Check if the node has become empty
        if (nodeHeader->numberKeys == 0) {
            disposeFlag = true;

            // If the node is ROOT
            if (type == ROOT) {
                indexHeader.rootPage = IX_NO_PAGE;
                headerModified = TRUE;
            }
            else {
                // Make recursive call with the parent node
                if ((rc = pushDeletionUp(nodeHeader->parent, node))) {
                    return rc;
                }
            }
        }

        // Copy the data to the node page
        memcpy(nodeData, (char*) nodeHeader, sizeof(IX_NodeHeader));
        memcpy(valueData, (char*) valueArray, sizeof(IX_NodeValue)*(degree+1));
    }


    // Unpin the node page
    if ((rc = pfFH.UnpinPage(node))) {
        return rc;
    }

    // Dispose the page if dispose flag is true
    if (disposeFlag) {
        if ((rc = pfFH.DisposePage(node))) {
            return rc;
        }
    }

    // Return OK
    return OK_RC;
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

// Method: satisfiesInterval(T key1, T key2, T value)
// Boolean whether the value satisfies the given interval
template<typename T>
bool IX_IndexHandle::satisfiesInterval(T key1, T key2, T value) {
    return (value >= key1 && value < key2);
}
