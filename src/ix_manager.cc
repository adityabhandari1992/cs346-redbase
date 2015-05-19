//
// File:        ix_manager.cc
// Description: IX_Manager class implementation
// Authors:     Aditya Bhandari (adityasb@stanford.edu)
//

#include "ix_internal.h"
#include "ix.h"
#include <string>
#include <cstring>
#include <cstdio>
#include <sstream>
#include <iostream>
using namespace std;

// Constructor
IX_Manager::IX_Manager(PF_Manager &pfm) {
    // Store the PF_Manager
    this->pfManager = &pfm;
}

// Destructor
IX_Manager::~IX_Manager() {
    // Nothing to free
}

// Method: CreateIndex(const char *fileName, int indexNo, AttrType attrType, int attrLength)
// Create a new Index for the given file name
/* Steps:
    1) Check whether the index number is valid (non negative)
    2) Generate index filename = fileName.indexNo
    3) Check the attribute type and length consistency
    4) Create index file using the PF Manager
    5) Allocate a header page by opening the file
    6) Get the header page number and mark page as dirty
    7) Create a index header object
    8) Copy the index header to the index header page
    9) Unpin page and flush to disk
    10) Close the opened file
*/
RC IX_Manager::CreateIndex(const char *fileName, int indexNo,
                           AttrType attrType, int attrLength) {
    // Validate the index
    if (indexNo < 0) {
        return IX_NEGATIVE_INDEX;
    }

    // Check filename
    if (fileName == NULL) {
        return IX_NULL_FILENAME;
    }

    // Declare an integer for the return code
    int rc;

    // Generate the index filename
    string fileNameString = generateIndexFileName(fileName, indexNo);
    const char* indexFileName = fileNameString.c_str();

    // Check attributes
    if (attrType != INT && attrType != FLOAT && attrType != STRING) {
        return IX_INVALID_ATTRIBUTE;
    }

    // Check the attribute type and length consistencies
    if ((attrType == INT || attrType == FLOAT) && attrLength != 4) {
        return IX_INCONSISTENT_ATTRIBUTE;
    }
    if (attrType == STRING && (attrLength < 1 || attrLength > MAXSTRINGLEN)) {
        return IX_INCONSISTENT_ATTRIBUTE;
    }


    // Create index file using the PF Manager
    if ((rc = pfManager->CreateFile(indexFileName))) {
        return rc;
    }

    // Create a file handle for the created file
    PF_FileHandle pfFH;

    // Open the file
    if ((rc = pfManager->OpenFile(indexFileName, pfFH))) {
        return rc;
    }

    // Create a page handle for the new page
    PF_PageHandle pfPH;

    // Allocate a page in the created file
    if ((rc = pfFH.AllocatePage(pfPH))) {
        return rc;
    }

    // Get the data in the newly allocated page
    char* pData;
    if ((rc = pfPH.GetData(pData))) {
        return rc;
    }

    // Get the page number of the header page
    PageNum pNum;
    if ((rc = pfPH.GetPageNum(pNum))) {
        return rc;
    }

    // Mark the header page as dirty
    if ((rc = pfFH.MarkDirty(pNum))) {
        return rc;
    }

    // Create an index header
    IX_IndexHeader* indexHeader = new IX_IndexHeader;

    // Set the index header fields
    indexHeader->attrType = attrType;
    indexHeader->attrLength = attrLength;
    indexHeader->rootPage = IX_NO_PAGE;
    indexHeader->degree = findDegreeOfNode(attrLength);

    // Copy the index header in the header page
    char* fileData = (char*) indexHeader;
    memcpy(pData, fileData, sizeof(IX_IndexHeader));

    // Delete the index header object
    delete indexHeader;

    // Unpin the page
    if ((rc = pfFH.UnpinPage(pNum))) {
        return rc;
    }

    // Flush the page to disk
    if ((rc = pfFH.ForcePages(pNum))) {
        return rc;
    }

    // Close the file using the PF Manager
    if ((rc = pfManager->CloseFile(pfFH))) {
        return rc;
    }

    // Return OK
    return OK_RC;
}

// Method: DestroyIndex(const char *fileName, int indexNo)
// Destroy an Index
/* Steps:
    1) Generate the index filename = fileName.indexNo
    2) Destroy the index file using the PF Manager
*/
RC IX_Manager::DestroyIndex(const char *fileName, int indexNo) {
    // Check the filename and index number
    if (fileName == NULL) {
        return IX_NULL_FILENAME;
    }

    if (indexNo < 0) {
        return IX_NEGATIVE_INDEX;
    }

    // Declare an integer for the return code
    int rc;

    // Generate the index file name
    string fileNameString = generateIndexFileName(fileName, indexNo);
    const char* indexFileName = fileNameString.c_str();

    // Destroy the file using the PF Manager
    if ((rc = pfManager->DestroyFile(indexFileName))) {
        return rc;
    }

    // Return OK
    return OK_RC;
}

// Method: OpenIndex(const char *fileName, int indexNo, IX_IndexHandle &indexHandle)
// Open an Index
/* Steps:
    1) Check if the index handle is already open
    2) Open the file using the PF Manager
    3) Update the index handle members
    4) Store the index header in memory
        - Create a PF PageHandle to the header page
        - Get the data from the header page
        - Store the data in an IX_IndexHeader object
    5) Unpin the index header page
*/
RC IX_Manager::OpenIndex(const char *fileName, int indexNo, IX_IndexHandle &indexHandle) {
    // Check if the index handle is already open
    if (indexHandle.isOpen) {
        return IX_INDEX_OPEN;
    }

    // Check the filename and index number
    if (fileName == NULL) {
        return IX_NULL_FILENAME;
    }

    if (indexNo < 0) {
        return IX_NEGATIVE_INDEX;
    }

    // Declare an integer for the return code
    int rc;

    // Generate the index file name
    string fileNameString = generateIndexFileName(fileName, indexNo);
    const char* indexFileName = fileNameString.c_str();

    // Declare a PF FileHandle
    PF_FileHandle pfFH;

    // Open the file using the PF Manager
    if ((rc = pfManager->OpenFile(indexFileName, pfFH))) {
        return rc;
    }

    // Update the index handle members
    indexHandle.pfFH = pfFH;
    indexHandle.headerModified = FALSE;

    // Initialize the last deleted entry
    indexHandle.lastDeletedEntry.keyValue = NULL;
    indexHandle.lastDeletedEntry.rid = dummyRID;

    // Store the index header in memory
    // Create PF page handle for the header page
    PF_PageHandle pfPH;
    PageNum headerPageNum;
    if ((rc = pfFH.GetFirstPage(pfPH))) {
        return rc;
    }
    if ((rc = pfPH.GetPageNum(headerPageNum))) {
        return rc;
    }

    // Get the data from the header page
    char* pData;
    if ((rc = pfPH.GetData(pData))) {
        return rc;
    }
    if ((rc = pfFH.MarkDirty(headerPageNum))) {
        return rc;
    }

    // Store the data in the index header object
    IX_IndexHeader* iH = (IX_IndexHeader*) pData;
    memcpy(&indexHandle.indexHeader, iH, sizeof(IX_IndexHeader));

    // Unpin the header page
    if ((rc = pfFH.UnpinPage(headerPageNum))) {
        return rc;
    }
    indexHandle.isOpen = TRUE;

    // Return OK
    return OK_RC;
}

// Method: CloseIndex(IX_IndexHandle &indexHandle)
// Close an Index
/* Steps:
    1) Check if the index handle is open
    2) Update the index header page
        - Get the PF page handle to the header page
        - Copy the modified data to the header page
        - Unpin and flush the header page
    3) Close the index file using the PF Manager
*/
RC IX_Manager::CloseIndex(IX_IndexHandle &indexHandle) {
    // Check if the index file is open
    if (!indexHandle.isOpen) {
        return IX_INDEX_CLOSED;
    }

    // Declare an integer for the return code
    int rc;

    // Get the PF FileHandle
    PF_FileHandle pfFH = indexHandle.pfFH;
    PF_PageHandle pfPH;
    PageNum headerPageNum;

    // Update the index header if it is modified
    if (indexHandle.headerModified) {
        // Get the page handle for the header page
        if ((rc = pfFH.GetFirstPage(pfPH))) {
            return rc;
        }

        // Get the page number of the header page
        if ((rc = pfPH.GetPageNum(headerPageNum))) {
            return rc;
        }
        if ((rc = pfFH.MarkDirty(headerPageNum))) {
            return rc;
        }

        // Get the data from the header page
        char* pData;
        if ((rc = pfPH.GetData(pData))) {
            return rc;
        }

        // Copy the modified index header
        char* newPData = (char*) &indexHandle.indexHeader;
        memcpy(pData, newPData, sizeof(IX_IndexHeader));

        // Unpin the header page
        if ((rc = pfFH.UnpinPage(headerPageNum))) {
            return rc;
        }

        // Flush the modified header page
        if ((rc = pfFH.ForcePages(headerPageNum))) {
            return rc;
        }
    }

    // Free the last scanned entry array
    char* temp = static_cast<char*> (indexHandle.lastDeletedEntry.keyValue);
    delete[] temp;

    // Close the file using the PF Manager
    if ((rc = pfManager->CloseFile(pfFH))) {
        return rc;
    }

    // Update the flags
    indexHandle.isOpen = FALSE;
    indexHandle.headerModified = FALSE;

    // Return OK
    return OK_RC;
}


// Method: generateIndexFileName(const char* fileName, int indexNo)
// Generate a const char* = fileName.indexNo
string IX_Manager::generateIndexFileName(const char* fileName, int indexNo) {
    stringstream convert;
    convert << fileName << "." << indexNo;
    return convert.str();
}

// Method: findDegreeOfNode(int attrLength)
// Find the maximum degree of node that can fit in a page
int IX_Manager::findDegreeOfNode(int attrLength) {
    int headerSize = sizeof(IX_NodeHeader);
    int n = 1;
    while(true) {
        int size = headerSize + n*attrLength + (n+1)*sizeof(IX_NodeValue);
        if (size > PF_PAGE_SIZE) break;
        n++;
    }
    return (n-1);
}