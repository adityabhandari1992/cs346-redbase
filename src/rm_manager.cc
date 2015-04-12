//
// File:        rm_manager.cc
// Description: RM_Manager class implementation
// Authors:     Aditya Bhandari (adityasb@stanford.edu)
//

#include <iostream>
#include <cstring>
#include <string>
#include "rm.h"
using namespace std;

// Constructor
RM_Manager::RM_Manager(PF_Manager &pfm) {
    // Copy the PF Manager object to a local object
    this->pfManager = &pfm;
}

// Destructor
RM_Manager::~RM_Manager() {
    // Delete the PF Manager object
    // delete pfManager;
}


// Method: CreateFile(const char *fileName, int recordSize)
// Create a file with the given filename and record size
/* Steps:
    1) Check for valid record size
    2) Create file using the PF Manager
    3) Allocate a header page by opening the file
    4) Get the header page number and mark page as dirty
    5) Create a file header object
    6) Copy the file header to the file header page
    7) Unpin page and flush to disk
*/
RC RM_Manager::CreateFile(const char *fileName, int recordSize) {
    // Check for a valid record size
    if (recordSize < 0) {
        return RM_SMALL_RECORD;
    }
    if (recordSize > PF_PAGE_SIZE) {
        return RM_LARGE_RECORD;
    }

    // Declare an integer for the return code
    int rc;
    if ((rc = pfManager->CreateFile(fileName))) {
        // Return the same error from the PF manager
        return rc;
    }

    // Create a file handle for the created file
    PF_FileHandle pfFH;

    // Open the file
    if ((rc = pfManager->OpenFile(fileName, pfFH))) {
        // Return the error code from the PF Manager
        return rc;
    }

    // Create a page handle for the new page
    PF_PageHandle pfPH;

    // Allocate a page in the created file
    if ((rc = pfFH.AllocatePage(pfPH))) {
        // Return the error code from the PF FileHandle
        return rc;
    }

    // Get the data in the newly allocated page
    char* pData;
    if ((rc = pfPH.GetData(pData))) {
        // Return the error from the PF PageHandle
        return rc;
    }

    // Get the page number of the header page
    PageNum pNum;
    if ((rc = pfPH.GetPageNum(pNum))) {
        // Return the error from the PF PageHandle
        return rc;
    }

    // Mark the header page as dirty
    if ((rc = pfFH.MarkDirty(pNum))) {
        // Return the error from the PF FileHandle
        return rc;
    }

    // Create a file header
    RM_FileHeaderPage* fileHeader = new RM_FileHeaderPage;

    // Set the file header fields
    fileHeader->recordSize = recordSize;
    fileHeader->numberRecordsOnPage = findNumberRecords(recordSize);
    fileHeader->numberPages = 0;
    fileHeader->firstFreePage = NO_FREE_PAGE;

    // Copy the file header in the header page
    char* fileData = (char*) fileHeader;
    memcpy(pData, fileData, sizeof(RM_FileHeaderPage));

    // Delete the file header object
    delete fileHeader;

    // Unpin the page
    if ((rc = pfFH.UnpinPage(pNum))) {
        // Return the error from the PF FileHandle
        return rc;
    }

    // Flush the page to disk
    if ((rc = pfFH.ForcePages(pNum))) {
        // Return the error from the PF FileHandle
        return rc;
    }

    // Return OK
    return OK_RC;
}

// Method: DestroyFile(const char *fileName)
// Destroy the file with the given filename
/* Steps:
    1) Destroy the file using the PF Manager
*/
RC RM_Manager::DestroyFile(const char *fileName) {
    // Declare an integer for the return code
    int rc;

    // Destroy the file using the PF Manager
    if ((rc = pfManager->DestroyFile(fileName))) {
        // Return the error from the PF Manager
        return rc;
    }

    // Return OK
    return OK_RC;
}


// Method: OpenFile(const char *fileName, RM_FileHandle &fileHandle)
// Open the file with the given filename with the specified filehandle
/* Steps:
    1) Check if the file handle is already open
    2) Open the file using the PF Manager
    3) Update the file handle members
    4) Store the file header in memory
        - Create a PF PageHandle to the header page
        - Get the data from the header page
        - Store the data in a RM FileHeaderPage object
    5) Unpin the file header page
*/
RC RM_Manager::OpenFile(const char *fileName, RM_FileHandle &fileHandle) {
    // Check if the file handle is already open
    if (fileHandle.isOpen) {
        return RM_FILE_OPEN;
    }

    // Declare an integer for the return code
    int rc;

    // Declare a PF FileHandle
    PF_FileHandle pfFH;

    // Open the file using the PF Manager
    if ((rc = pfManager->OpenFile(fileName, pfFH))) {
        // Return the error from the PF Manager
        return rc;
    }

    // Set the PF file handle in the RM file handle
    fileHandle.pfFH = pfFH;

    // Set the file handle open flag to true
    fileHandle.isOpen = TRUE;

    // Set the modified flag to false
    fileHandle.headerModified = FALSE;

    // Store the file header information in memory
    // Get the page handle for the first page
    PF_PageHandle pfPH;
    if ((rc = pfFH.GetFirstPage(pfPH))) {
        // Return the error from the PF FileHandle
        return rc;
    }

    // Get data from the first page
    char* pData;
    if ((rc = pfPH.GetData(pData))) {
        // Return the error from the PF PageHandle
        return rc;
    }

    // Store the data in the file header object
    RM_FileHeaderPage* fH = (RM_FileHeaderPage*) pData;
    memcpy(&fileHandle.fileHeader, fH, sizeof(RM_FileHeaderPage));

    // Unpin the header page
    PageNum headerPageNum;
    if ((rc = pfPH.GetPageNum(headerPageNum))) {
        // Return the error from the PF PageHandle
        return rc;
    }
    if ((rc = pfFH.UnpinPage(headerPageNum))) {
        // Return the error from the PF FileHandle
        return rc;
    }

    // Return OK
    return OK_RC;
}


// Method: CloseFile(RM_FileHandle &fileHandle)
// Close the file with the given filehandle
/* Steps:
    1) Check if the file handle is open
    2) Update the file header page if modified
        - Get the PF PageHandle for the header page
        - Copy the modified data to the header page
        - Unpin the header page
        - Flush/force the page on to the disk
    3) Unpin all the file pages
    4) Close the file using the PF Manager
*/
RC RM_Manager::CloseFile(RM_FileHandle &fileHandle) {
    // Check if the file handle refers to an open file
    if (!fileHandle.isOpen) {
        return RM_FILE_CLOSED;
    }

    // Declare an integer for the return code
    int rc;

    // Get the PF FileHandle
    PF_FileHandle pfFH = fileHandle.pfFH;
    PF_PageHandle pfPH;
    PageNum pNum;

    // Update the file header if it is modified
    if (fileHandle.headerModified) {
        // Get the page handle for the header page
        if ((rc = pfFH.GetFirstPage(pfPH))) {
            // Return the error from the PF FileHandle
            return rc;
        }

        // Get the page number of the header page
        if ((rc = pfPH.GetPageNum(pNum))) {
            // Return the error from the PF PageHandle
            return rc;
        }

        // Get the data from the header page
        char* pData;
        if ((rc = pfPH.GetData(pData))) {
            // Return the error from the PF PageHandle
            return rc;
        }

        // Copy the modified page header to pData
        char* pD = (char*) &fileHandle.fileHeader;
        memcpy(pData, pD, sizeof(RM_FileHeaderPage));


        // Flush the modified header page
        if ((rc = pfFH.ForcePages(pNum))) {
            // Return the error from the PF FileHandle
            return rc;
        }

        // Unpin the header page
        if ((rc = pfFH.UnpinPage(pNum))) {
            // Return the error from the PF FileHandle
            return rc;
        }
    }

    // Close the file using the PF Manager
    if ((rc = pfManager->CloseFile(pfFH))) {
        cout << "Oops! Entered here!" << endl;
        PF_PrintError(rc);
        // Return the error from the PF Manager
        return rc;
    }

    // Update the flags
    fileHandle.isOpen = FALSE;
    fileHandle.headerModified = FALSE;

    // Return OK
    return OK_RC;
}


// Method: findNumberRecords(int recordSize)
// Find the number of records that can fit in a page
int RM_Manager::findNumberRecords(int recordSize) {
    int headerSize = sizeof(RM_PageHeader);
    int n = 1;
    while(true) {
        int bitmapSize = n/8;
        if (n%8 != 0) bitmapSize++;
        int size = headerSize + bitmapSize + n*recordSize;
        if (size > PF_PAGE_SIZE) break;
        n++;
    }
    return (n-1);
}