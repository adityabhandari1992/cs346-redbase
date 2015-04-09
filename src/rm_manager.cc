//
// File:        rm_manager.cc
// Description: RM_Manager class implementation
// Authors:     Aditya Bhandari (adityasb@stanford.edu)
//

#include "rm.h"
using namespace std;

// Constructor
RM_Manager::RM_Manager(PF_Manager &pfm) {
    // Copy the PF Manager object to a local object
    this->pfManager = pfm;
}

// Destructor
RM_Manager::~RM_Manager() {
    // Delete the PF Manager object
    delete pfManager;
}

// Create a file with the given filename and record size
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
    if (rc = pfm.CreateFile(filename)) {
        // Return the same error from the PF manager
        return rc;
    }

    // If the file is created, add a header page

    // Create a file handle for the created file
    PF_FileHandle pfFH;

    // Open the file
    if (rc = pfm.OpenFile(filename, pfFH)) {
        // Return the error code from the PF Manager
        return rc;
    }

    // Create a page handle for the new page
    PF_PageHandle pfPH;

    // Allocate a page in the created file
    if (rc = pfFH.AllocatePage(pfPH)) {
        // Return the error code from the PF FileHandle
        return rc;
    }

    // Get the data in the newly allocated page
    char* pData;
    if (rc = pfPH.GetData(pData)) {
        // Return the error from the PF PageHandle
        return rc;
    }

    // Get the page number of the header page
    PageNum pNum;
    if (rc = pfPH.GetPageNum(pNum)) {
        // Return the error from the PF PageHandle
        return rc;
    }

    // Mark the header page as dirty
    if (rc = pfFH.MarkDirty(pNum)) {
        // Return the error from the PF FileHandle
        return rc;
    }

    // Modify the data in the header page
    // TODO
    // strcpy(pData, recordSize);
    // strcat(pData, );

    // Return OK
    return OK_RC;
}

// Destroy the file with the given filename
RC RM_Manager::DestroyFile(const char *fileName) {
    // Declare an integer for the return code
    int rc;

    // Destroy the file using the PF Manager
    if (rc = pfm.DestroyFile(filename)) {
        // Return the error from the PF Manager
        return rc;
    }

    // Return OK
    return OK_RC;
}

// Open the file with the given filename with the specified filehandle
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
    if (rc = pfm.OpenFile(filename, pfFH)) {
        // Return the error from the PF Manager
        return rc;
    }

    // Set the PF file handle in the RM file handle
    fileHandle.pfFH = pfFH;

    // Set the file handle open flag to true
    fileHandle.isOpen = TRUE;

    // Set the modified flag to false
    fileHandle.headerModified = FALSE;

    // Store the file header information
    // Get the page handle for the first page
    PF_PageHandle pfPH;
    if (rc = pfFH.GetFirstPage(pfPH)) {
        // Return the error from the PF FileHandle
        return rc;
    }

    // Get data from the first page
    char* pData;
    if (rc = pfPH.GetData(pData)) {
        // Return the error from the PF PageHandle
        return rc;
    }

    // Store the data in the header variable
    fileHandle.fileHeader = pData;

    // Return OK
    return OK_RC;
}

// Close the file with the given filehandle
RC RM_Manager::CloseFile(RM_FileHandle &fileHandle) {
    // Check if the file handle refers to an open file
    if (!fileHandle.isOpen) {
        return RM_FILE_CLOSED;
    }

    // Declare an integer for the return code
    int rc;

    // Get the PF FileHandle
    PF_FileHandle pfFH = fileHandle.pfFH;

    // Update the file header if it is modified
    if (fileHandle.headerModified) {
        // Get the page handle for the header page
        PF_PageHandle pfPH;
        if (rc = pfFH.GetFirstPage(pfPH)) {
            // Return the error from the PF FileHandle
            return rc;
        }

        // Get the page number of the header page
        PageNum pNum;
        if (rc = pfPH.GetPageNum(pNum)) {
            // Return the error from the PF PageHandle
            return rc;
        }

        // Get the data from the header page
        char* pData
        if (rc = pfPH.GetData(pData)) {
            // Return the error from the PF PageHandle
            return rc;
        }

        // Copy the modified page header to pData
        pData = fileHandle.fileHeader;

        // Flush the modified header page
        if (rc = pfFH.ForcePages(pNum)) {
            // Return the error from the PF FileHandle
            return rc;
        }
    }

    // Close the file using the PF Manager
    if (rc = pfm.CloseFile(fileHandle)) {
        // Return the error from the PF Manager
        return rc;
    }

    // Return OK
    return OK_RC;
}