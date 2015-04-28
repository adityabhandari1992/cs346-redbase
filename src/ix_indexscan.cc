//
// File:        ix_indexscan.cc
// Description: IX_IndexScan class implementation
// Authors:     Aditya Bhandari (adityasb@stanford.edu)
//

#include "ix_internal.h"
#include "ix.h"
#include <iostream>
#include <cstring>
using namespace std;

// Constructor
IX_IndexScan::IX_IndexScan() {
    // Set open scan flag to false
    scanOpen = FALSE;
}

// Destructor
IX_IndexScan::~IX_IndexScan() {
    // Nothing to free
}

// Method: OpenScan(const IX_IndexHandle &indexHandle, CompOp compOp,
//                  void *value, ClientHint  pinHint)
// Open index scan
/* Steps:
    1) Check for erroneous input
    2) Initialize the class variables
        - Store attrType, attrLength, compOp, value, pinHint and degree
    3) Get the first key satisfying the condition and store
*/
RC IX_IndexScan::OpenScan(const IX_IndexHandle &indexHandle, CompOp compOp,
                          void *value, ClientHint  pinHint) {
    // Check for erroneous input
    if (!indexHandle.isOpen) {
        return IX_INDEX_CLOSED;
    }

    if (compOp != NO_OP && compOp != EQ_OP && compOp != LT_OP &&
        compOp != GT_OP && compOp != LE_OP && compOp != GE_OP) {
        return IX_INVALID_OPERATOR;
    }

    // If the value is a null pointer
    if (compOp != NO_OP && value == NULL) {
        return IX_INVALID_OPERATOR;
    }

    // Store the class variables
    this->indexHandle = &indexHandle;
    this->attrType = (indexHandle.indexHeader).attrType;
    this->attrLength = (indexHandle.indexHeader).attrLength;
    this->compOp = compOp;
    this->value = value;
    this->pinHint = pinHint;
    this->degree = (indexHandle.indexHeader).degree;
    this->inBucket = FALSE;
    this->bucketPosition = 0;
    (this->lastScannedEntry).keyValue = NULL;
    (this->lastScannedEntry).rid = dummyRID;

    // Declare an integer for return code
    int rc;

    // Set the scan open flag
    scanOpen = TRUE;

    // Get to the first leaf node satisfying the condition
    PageNum rootPage = (indexHandle.indexHeader).rootPage;

    // If the root does not exist (empty index)
    if (rootPage == IX_NO_PAGE) {
        this->pageNumber = IX_NO_PAGE;
        this->keyPosition = -1;
    }

    // Else get the first page
    else {
        PageNum firstPage;
        int position;
        if ((rc = SearchEntry(rootPage, firstPage, position))) {
            return rc;
        }

        // Store the page number and key positions
        this->pageNumber = firstPage;
        this->keyPosition = position;
    }

    // Return OK
    return OK_RC;
}

// Method: GetNextEntry(RID &rid)
// Get the next matching entry
// Return IX_EOF if no more matching entries
/* Steps:
    1) If current page is IX_NO_PAGE, return IX_EOF
    2) Get the data from the current page
    3) If the last scanned entry is deleted
        - If in bucket and position = 0, update page number to the parent node
        - If in bucket and position != 0, update bucket position
        - If not in bucket and position != 0, update key position
    4) If in bucket
        - Go the bucket position and set rid to stored RID
        - If bucket position = capacity, go to parent node and increment key position (go to next page in case of last key)
    5) Else if not in bucket
        - Go to the key position and check if it satisfies the condition
    6) If it satisfies the condition
        - Get the RID and assign it to rid
        - If bucket is IX_NO_PAGE, increment key position (go to next page in case of last key)
        - Else set inBucket to TRUE and bucket position to 0
    7) Return OK
*/
RC IX_IndexScan::GetNextEntry(RID &rid) {
    // Return error if the scan is closed
    if (!scanOpen) {
        return IX_SCAN_CLOSED;
    }

    // Check the current page
    if (pageNumber == IX_NO_PAGE) {
        return IX_EOF;
    }

    // Declare an integer for the return code
    int rc;

    // Get the data from the current page
    PF_FileHandle pfFH = indexHandle->pfFH;
    PF_PageHandle pfPH;
    char* pageData;
    if ((rc = pfFH.GetThisPage(pageNumber, pfPH))) {
        return rc;
    }
    if ((rc = pfPH.GetData(pageData))) {
        return rc;
    }

    // If the last scanned entry exists
    if (!compareRIDs(lastScannedEntry.rid, dummyRID)) {
        // Check if the last scanned entry was deleted
        if (!compareRIDs((indexHandle->lastDeletedEntry).rid, dummyRID) && compareEntries(lastScannedEntry, indexHandle->lastDeletedEntry)) {
            if ((rc = pfFH.UnpinPage(pageNumber))) {
                return rc;
            }
        }

        // Else if this is not the first entry scanned, update the variables
        else {
            if (inBucket) {
                IX_BucketPageHeader* bucketHeader = (IX_BucketPageHeader*) pageData;
                int numberRecords = bucketHeader->numberRecords;

                bucketPosition++;

                // Unpin the bucket page
                if ((rc = pfFH.UnpinPage(pageNumber))) {
                    return rc;
                }

                // If end of the bucket
                if (bucketPosition == numberRecords) {
                    inBucket = FALSE;
                    bucketPosition = 0;

                    // Set the new page number and key position
                    pageNumber = bucketHeader->parentNode;
                    keyPosition++;

                    // Get the page data
                    if ((rc = pfFH.GetThisPage(pageNumber, pfPH))) {
                        return rc;
                    }
                    if ((rc = pfPH.GetData(pageData))) {
                        return rc;
                    }

                    IX_NodeHeader* nodeHeader = (IX_NodeHeader*) pageData;
                    int numberKeys = nodeHeader->numberKeys;
                    char* valueData = pageData + sizeof(IX_NodeHeader) + attrLength*degree;
                    IX_NodeValue* valueArray = (IX_NodeValue*) valueData;

                    // Unpin the page
                    if ((rc = pfFH.UnpinPage(pageNumber))) {
                        return rc;
                    }

                    // If end of the node
                    if (keyPosition == numberKeys) {
                        pageNumber = valueArray[degree].page;
                        keyPosition = 0;
                    }
                }
            }
            else {
                IX_NodeHeader* nodeHeader = (IX_NodeHeader*) pageData;
                int numberKeys = nodeHeader->numberKeys;
                char* keyData = pageData + sizeof(IX_NodeHeader);
                char* valueData = keyData + attrLength*degree;
                IX_NodeValue* valueArray = (IX_NodeValue*) valueData;

                // Unpin the page
                if ((rc = pfFH.UnpinPage(pageNumber))) {
                    return rc;
                }

                // Go to the next position
                // If no bucket
                if (valueArray[keyPosition].page == IX_NO_PAGE) {
                    keyPosition++;

                    // If end of a node
                    if (keyPosition == numberKeys) {
                        pageNumber = valueArray[degree].page;
                        keyPosition = 0;
                    }
                }
                else {
                    pageNumber = valueArray[keyPosition].page;
                    inBucket = TRUE;
                    bucketPosition = 0;
                }
            }
        }

        // Check that we have not reached the end of file
        if (pageNumber == IX_NO_PAGE) {
            return IX_EOF;
        }

        if ((rc = pfFH.GetThisPage(pageNumber, pfPH))) {
            return rc;
        }
        if ((rc = pfPH.GetData(pageData))) {
            return rc;
        }
    }

    // If in bucket
    if (inBucket) {
        // Go to the bucket position and get RID
        RID* ridList = (RID*) (pageData + sizeof(IX_BucketPageHeader));
        rid = ridList[bucketPosition];

        // Unpin the bucket page
        if ((rc = pfFH.UnpinPage(pageNumber))) {
            return rc;
        }
    }

    // else if not in bucket
    else {
        IX_NodeHeader* nodeHeader = (IX_NodeHeader*) pageData;
        int numberKeys = nodeHeader->numberKeys;
        char* keyData = pageData + sizeof(IX_NodeHeader);
        char* valueData = keyData + attrLength*degree;
        IX_NodeValue* valueArray = (IX_NodeValue*) valueData;

        // Unpin the current page
        if ((rc = pfFH.UnpinPage(pageNumber))) {
            return rc;
        }
        bool unpinned = true;

        // Check if the current key satisfies the condition
        while (true) {
            if (attrType == INT) {
                int* keyArray = (int*) keyData;
                int givenValue = *static_cast<int*>(value);
                if(satisfiesCondition(keyArray[keyPosition], givenValue))
                    break;
            }
            else if (attrType == FLOAT) {
                float* keyArray = (float*) keyData;
                float givenValue = *static_cast<float*>(value);
                if (satisfiesCondition(keyArray[keyPosition], givenValue))
                    break;
            }
            else {
                char* keyArray = (char*) keyData;
                char* givenValueChar = static_cast<char*>(value);
                string givenValue(givenValueChar);
                string currentKey(keyArray + keyPosition*attrLength);
                if (satisfiesCondition(currentKey, givenValue))
                    break;
            }

            // Go to the next key position
            keyPosition++;

            // If end of a node
            if (keyPosition == numberKeys) {
                PageNum previousPage = pageNumber;
                pageNumber = valueArray[degree].page;
                keyPosition = 0;

                // Unpin the previous page
                if (!unpinned) {
                    if ((rc = pfFH.UnpinPage(previousPage))) {
                       return rc;
                    }
                }

                if (pageNumber == IX_NO_PAGE) {
                    return IX_EOF;
                }
                else {
                    // Get the new page
                    if ((rc = pfFH.GetThisPage(pageNumber, pfPH))) {
                        return rc;
                    }
                    if ((rc = pfPH.GetData(pageData))) {
                        return rc;
                    }
                    unpinned = false;

                    nodeHeader = (IX_NodeHeader*) pageData;
                    numberKeys = nodeHeader->numberKeys;
                    keyData = pageData + sizeof(IX_NodeHeader);
                    valueData = keyData + attrLength*degree;
                    valueArray = (IX_NodeValue*) valueData;
                }
            }
        }

        // Get the RID and assign to rid
        rid = valueArray[keyPosition].rid;
    }

    // Update the last scanned entry
    if (lastScannedEntry.keyValue == NULL) {
        lastScannedEntry.keyValue = new char[attrLength];
    }
    memcpy(lastScannedEntry.keyValue, value, attrLength);
    lastScannedEntry.rid = rid;

    // Return OK
    return OK_RC;
}

// Method: CloseScan()
// Close index scan
/* Steps:
    1) Check if the scan is open
    2) Update the scan open flag
*/
RC IX_IndexScan::CloseScan() {
    // Return error if the scan is closed
    if (!scanOpen) {
        return IX_SCAN_CLOSED;
    }

    // Set scan open flag to FALSE
    scanOpen = FALSE;

    // Free the last scanned entry array
    char* temp = static_cast<char*> (lastScannedEntry.keyValue);
    delete[] temp;

    // Return OK
    return OK_RC;
}

// Method: SearchEntry(PageNum node, PageNum &pageNumber, int &keyPosition)
// Recursively search for an index entry
/* Steps:
    1) Get the data in the node page
    2) Check the type of node
    3) If leaf node
        - Search for the value in the node
        - Get the correspoding key position
        - Unpin the current node page
    4) If root/internal node
        - Search for corresponding pointer to next node
        - Unpin the current node page
        - Recursive call with the next node page
*/
RC IX_IndexScan::SearchEntry(PageNum node, PageNum &pageNumber, int &keyPosition) {
    // Declare an integer for the return code
    int rc;

    if (node == IX_NO_PAGE) {
        pageNumber = IX_NO_PAGE;
        keyPosition = -1;
        return OK_RC;
    }

    // Get the data in the node page
    PF_FileHandle pfFH = indexHandle->pfFH;
    PF_PageHandle pfPH;
    if ((rc = pfFH.GetThisPage(node, pfPH))) {
        return rc;
    }
    char* nodeData;
    if ((rc = pfPH.GetData(nodeData))) {
        return rc;
    }

    // Get the node type
    IX_NodeHeader* nodeHeader = (IX_NodeHeader*) nodeData;
    IX_NodeType nodeType = nodeHeader->type;
    int numberKeys = nodeHeader->numberKeys;
    char* keyData = nodeData + sizeof(IX_NodeHeader);
    char* valueData = keyData + attrLength*degree;
    IX_NodeValue* valueArray = (IX_NodeValue*) valueData;

    // If leaf node
    if (nodeType == LEAF || nodeType == ROOT_LEAF) {
        bool found = false;
        if (attrType == INT) {
            int* keyArray = (int*) keyData;
            int intValue = *static_cast<int*>(value);
            for (int i=0; i<numberKeys; i++) {
                if (satisfiesCondition(keyArray[i], intValue)) {
                    pageNumber = node;
                    keyPosition = i;
                    found = true;
                    break;
                }
            }
        }
        else if (attrType == FLOAT) {
            float* keyArray = (float*) keyData;
            float floatValue = *static_cast<float*>(value);
            for (int i=0; i<numberKeys; i++) {
                if (satisfiesCondition(keyArray[i], floatValue)) {
                    pageNumber = node;
                    keyPosition = i;
                    found = true;
                    break;
                }
            }
        }
        else {
            char* keyArray = (char*) keyData;
            char* charValue = static_cast<char*>(value);
            string stringValue(charValue);
            for (int i=0; i<numberKeys; i++) {
                string currentKey(keyArray + i*attrLength);
                if (satisfiesCondition(currentKey, stringValue)) {
                    pageNumber = node;
                    keyPosition = i;
                    found = true;
                    break;
                }
            }
        }

        if (!found) {
            pageNumber = IX_NO_PAGE;
            keyPosition = -1;
        }

        // Unpin the node page
        if ((rc = pfFH.UnpinPage(node))) {
            return rc;
        }
    }

    // Else if it is an internal node find the next page
    else if (nodeType == NODE || nodeType == ROOT) {
        PageNum nextPage = IX_NO_PAGE;
        if (compOp == LT_OP || compOp == LE_OP) {
            nextPage = valueArray[0].page;
        }
        else {
            if (attrType == INT) {
                int* keyArray = (int*) keyData;
                int intValue = *static_cast<int*>(value);
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
                float floatValue = *static_cast<float*>(value);
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
                char* charValue = static_cast<char*>(value);
                string stringValue(charValue);
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
        }

        // Unpin the current page
        if ((rc = pfFH.UnpinPage(node))) {
            return rc;
        }

        // Recursive call with the next node
        if ((rc = SearchEntry(nextPage, pageNumber, keyPosition))) {
            return rc;
        }
    }

    // Return OK
    return OK_RC;
}

// Method: satisfiesCondition(T key, T value)
// Boolean whether the key satisfies the required condition
template<typename T>
bool IX_IndexScan::satisfiesCondition(T key, T value) {
    bool match = false;
    switch(compOp) {
        case EQ_OP:
            if (key == value) match = true;
            break;
        case LT_OP:
            if (key < value) match = true;
            break;
        case GT_OP:
            if (key > value) match = true;
            break;
        case LE_OP:
            if (key <= value) match = true;
            break;
        case GE_OP:
            if (key >= value) match = true;
            break;
        default:
            break;
    }
    return match;
}

// Method: satisfiesInterval(T key1, T key2, T value)
// Boolean whether the value satisfies the given interval
template<typename T>
bool IX_IndexScan::satisfiesInterval(T key1, T key2, T value) {
    return (value >= key1 && value < key2);
}

// Method: compareRIDs(RID &rid1, RID &rid2)
// Boolean whether the two RIDs are the same
bool IX_IndexScan::compareRIDs(const RID &rid1, const RID &rid2) {
    PageNum pageNum1, pageNum2;
    SlotNum slotNum1, slotNum2;

    rid1.GetPageNum(pageNum1);
    rid1.GetSlotNum(slotNum1);
    rid2.GetPageNum(pageNum2);
    rid2.GetSlotNum(slotNum2);

    return (pageNum1 == pageNum2 && slotNum1 == slotNum2);
}

bool IX_IndexScan::compareEntries(const IX_Entry &e1, const IX_Entry &e2) {
    bool keyMatch = false;
    if (e1.keyValue == NULL || e1.keyValue == NULL) {
        return false;
    }

    if (attrType == INT) {
        int key1 = *static_cast<int*> (e1.keyValue);
        int key2 = *static_cast<int*> (e2.keyValue);
        if (key1 == key2) keyMatch = true;
    }
    else if (attrType == FLOAT) {
        float key1 = *static_cast<float*> (e1.keyValue);
        float key2 = *static_cast<float*> (e2.keyValue);
        if (key1 == key2) keyMatch = true;

    }
    else {
        char* key1Char = static_cast<char*> (e1.keyValue);
        char* key2Char = static_cast<char*> (e2.keyValue);
        string key1(key1Char);
        string key2(key2Char);
        if (key1 == key2) keyMatch = true;
    }
    return keyMatch && compareRIDs(e1.rid, e2.rid);
}