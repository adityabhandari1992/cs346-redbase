//
// File:        rm_filehandle.cc
// Description: RM_FileHandle class implementation
// Authors:     Aditya Bhandari (adityasb@stanford.edu)
//

#include "rm.h"
using namespace std;

// Default constructor
RM_FileHandle::RM_FileHandle() {
    // Set open flag to false
    isOpen = FALSE;

    // Set header modified flag to false
    headerModified = FALSE:
}

// Destructor
RM_FileHandle::~RM_FileHandle() {
    // Delete the PF file handle
    delete pfFH;
}

// Copy constructor
RM_FileHandle(const RM_FileHandle &fileHandle) {
    // Copy the PF file handle and open flag
    this->pfFH = fileHandle.pfFH;
    this->isOpen = fileHandle.isOpen;
}

// Overload =
RM_FileHandle& operator=(const RM_FileHandle &fileHandle) {
    // Check for self-assignment
    if (this != &fileHandle) {
        // Copy the PF file handle and open flag
        this->pfFH = fileHandle.pfFH;
        this->isOpen = fileHandle.isOpen;
    }

    // Return a reference to this
    return (*this);
}

// Given a RID, return the record
RC RM_FileHandle::GetRec(const RID &rid, RM_Record &rec) const {
    // Check if the file is open
    if (!isOpen) {
        return RM_FILE_CLOSED;
    }

    // Declare an integer for the return code
    int rc;

    // Get the page number for the required record
    PageNum pageNumber;
    if (rc = rid.GetPageNum(pageNumber)) {
        // Return the error from the RID class
        return rc;
    }

    // Get the slot number for the required record
    SlotNum slotNumber;
    if (rc = rid.GetSlotNum(slotNumber)) {
        // Return the error from the RID class
        return rc;
    }

    // Open the corresponding PF page handle
    PF_PageHandle pfPH;
    if (rc = pfFH.GetThisPage(pageNumber)) {
        // Return the error from the PF FileHandle
        return rc;
    }

    // Get the data from the page
    char* pData;
    if (rc = pfPH.GetData(pData)) {
        // Return the error from the PF PageHandle
        return rc;
    }

    // Get the record from the data using the slot number
    // Get the record size for the file
    // TODO

    // Check whether the slot number is valid
    // TODO

    // Get the record by using the record size
    RM_Record newRecord;

    // Set the data in the new record
    char* newPData;
    if (rc = newRecord.GetData(newPData)) {
        // Return the error from the RM Record
        return rc;
    }
    // TODO

    // Set the RID of the new record
    RID newRid;
    if (rc = newRecord.GetRid(newRid)) {
        // Return the error from the RM Record
        return rc;
    }
    newRid = rid;

    // Set valid flag of record to true
    newRecord.isValid = TRUE;

    // Set rec to point to the record
    rec = newRecord;

    // Return OK
    return OK_RC;
}

// Insert a new record
RC RM_FileHandle::InsertRec(const char *pData, RID &rid) {

}

// Delete a record
RC RM_FileHandle::DeleteRec(const RID &rid) {

}

// Update a record
RC RM_FileHandle::UpdateRec(const RM_Record &rec) {
    // Check if the file is open
    if (!isOpen) {
        return RM_FILE_CLOSED;
    }

    // Declare an integer for the return code
    int rc;

    // Get the RID for the record
    RID rid;
    if (rc = rec.GetRid(rid)) {
        // Return the error from the RM Record
        return rc;
    }

    // Get the page number for the required record
    PageNum pageNumber;
    if (rc = rid.GetPageNum(pageNumber)) {
        // Return the error from the RID class
        return rc;
    }

    // Get the slot number for the required record
    SlotNum slotNumber;
    if (rc = rid.GetSlotNum(slotNumber)) {
        // Return the error from the RID class
        return rc;
    }

    // Open the corresponding PF page handle
    PF_PageHandle pfPH;
    if (rc = pfFH.GetThisPage(pageNumber)) {
        // Return the error from the PF FileHandle
        return rc;
    }

    // Get the data from the page
    char* pData;
    if (rc = pfPH.GetData(pData)) {
        // Return the error from the PF PageHandle
        return rc;
    }

    // Get the record from the data using the slot number
    // Get the record size for the file
    // TODO

    // Check whether the slot number is valid
    // TODO

    // Get the record by using the record size
    // TODO

    // Get the data in the record rec
    char* newPData;
    if (rc = rec.GetData(newPData)) {
        // Return the error from the RM Record
        return rc;
    }

    // Set the data in the file to the new data
    // TODO
}

// Forces a page (along with any contents stored in this class)
// from the buffer pool to disk.  Default value forces all pages.
RC RM_FileHandle::ForcePages(PageNum pageNum = ALL_PAGES) {
    // Declare an integer for the return code
    int rc;

    // Force the pages using the PF FileHandle
    if (rc = pfFH.ForcePages(pageNum)) {
        // Return the error from the PF FileHandle
        return rc;
    }

    // Return OK
    return OK_RC;
}