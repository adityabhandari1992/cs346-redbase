//
// File:        ix_indexscan.cc
// Description: IX_IndexScan class implementation
// Authors:     Aditya Bhandari (adityasb@stanford.edu)
//

#include "ix_internal.h"
#include "ix.h"
#include <iostream>
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

    // If the value is a null pointer, set compOp to NO_OP
    if (compOp != NO_OP && value == NULL) {
        compOp = NO_OP;
    }

    // Store the class variables
    this->indexHandle = indexHandle;
    this->attrType = (indexHandle.indexHeader).attrType;
    this->attrLength = (indexHandle.indexHeader).attrLength;
    this->compOp = compOp;
    this->value = value;
    this->pinHint = pinHint;
    this->degree = (indexHandle.indexHeader).degree;
    this->inBucket = FALSE;
    this->bucketPosition = 0;

    // Set the scan open flag
    scanOpen = TRUE;

    // Declare an integer for return code
    int rc;

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
    3) If in bucket
        - Go the bucket position and set rid to stored RID
        - If bucket position = capacity, go to parent node and increment key position (go to next page in case of last key)
    4) Else if not in bucket
        - Go to the key position and check if it satisfies the condition
    5) If it satisfies the condition
        - Get the RID and assign it to rid
        - If bucket is IX_NO_PAGE, increment key position (go to next page in case of last key)
        - Else set inBucket to TRUE and bucket position to 0
    6) Return OK
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
    PF_FileHandle pfFH = indexHandle.pfFH;
    PF_PageHandle pfPH;
    char* pageData;
    if ((rc = pfFH.GetThisPage(pageNumber, pfPH))) {
        return rc;
    }
    if ((rc = pfPH.GetData(pageData))) {
        return rc;
    }

    // If in bucket
    if (inBucket) {
        IX_BucketPageHeader* bucketHeader = (IX_BucketPageHeader*) pageData;
        int numberRecords = bucketHeader->numberRecords;

        // Go to the bucket position and get RID
        RID* ridList = (RID*) (pageData + sizeof(IX_BucketPageHeader));
        rid = ridList[bucketPosition];

        // Set the next position
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

            // Unpin the current page
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
                string* keyArray = (string*) keyData;
                char* givenValueChar = static_cast<char*>(value);
                string givenValue = "";
                for (int i=0; i < attrLength; i++) {
                    givenValue += givenValueChar[i];
                }
                if (satisfiesCondition(keyArray[keyPosition], givenValue))
                    break;
            }

            // Go to the next key position
            keyPosition++;

            // If end of a node
            if (keyPosition == numberKeys) {
                pageNumber = valueArray[degree].page;
                keyPosition = 0;
                if (pageNumber == IX_NO_PAGE) return IX_EOF;
            }
        }

        // Get the RID and assign to rid
        rid = valueArray[keyPosition].rid;

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

    // Get the data in the node page
    PF_FileHandle pfFH = indexHandle.pfFH;
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
            string* keyArray = (string*) keyData;
            char* charValue = static_cast<char*>(value);
            string stringValue = "";
            for (int i=0; i < attrLength; i++) {
                stringValue += charValue[i];
            }
            for (int i=0; i<numberKeys; i++) {
                if (satisfiesCondition(keyArray[i], stringValue)) {
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
        PageNum nextPage;
        if (attrType == INT) {
            int* keyArray = (int*) keyData;
            int intValue = *static_cast<int*>(value);
            if (satisfiesInterval(0, keyArray[0], intValue)) {
                nextPage = valueArray[0].page;
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
            if (satisfiesInterval((float) 0, keyArray[0], floatValue)) {
                nextPage = valueArray[0].page;
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
            string* keyArray = (string*) keyData;
            char* charValue = static_cast<char*>(value);
            string stringValue = "";
            for (int i=0; i < attrLength; i++) {
                stringValue += charValue[i];
            }
            if (satisfiesInterval((string)"", keyArray[0], stringValue)) {
                nextPage = valueArray[0].page;
            }
            else {
                bool found;
                for (int i=1; i<numberKeys; i++) {
                    if (satisfiesInterval(keyArray[i-1], keyArray[i], stringValue)) {
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