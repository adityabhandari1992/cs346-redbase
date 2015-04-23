//
// File:        rm_filehandle.cc
// Description: RM_FileHandle class implementation
// Authors:     Aditya Bhandari (adityasb@stanford.edu)
//

#include <cstring>
#include <string>
#include "rm_internal.h"
#include "rm.h"
using namespace std;

// Default constructor
RM_FileHandle::RM_FileHandle() {
    // Set open flag to false
    isOpen = FALSE;

    // Set header modified flag to false
    headerModified = FALSE;

    // Initialize the file header
    fileHeader.numberPages = 0;
    fileHeader.firstFreePage = RM_NO_FREE_PAGE;
}

// Destructor
RM_FileHandle::~RM_FileHandle() {
    // Nothing to free
}

// Copy constructor
RM_FileHandle::RM_FileHandle(const RM_FileHandle &fileHandle) {
    // Copy the PF file handle, open flag, header modified flag and file header
    this->pfFH = fileHandle.pfFH;
    this->isOpen = fileHandle.isOpen;
    this->headerModified = fileHandle.headerModified;
    this->fileHeader = fileHandle.fileHeader;
}

// Overload =
RM_FileHandle& RM_FileHandle::operator=(const RM_FileHandle &fileHandle) {
    // Check for self-assignment
    if (this != &fileHandle) {
        // Copy the PF file handle, open flag, header modified flag and file header
        this->pfFH = fileHandle.pfFH;
        this->isOpen = fileHandle.isOpen;
        this->headerModified = fileHandle.headerModified;
        this->fileHeader = fileHandle.fileHeader;
    }

    // Return a reference to this
    return (*this);
}

// Method: GetRec(const RID &rid, RM_Record &rec) const
// Given a RID, return the record
/* Steps:
    1) Check if the file is open
    2) Get the page and slot numbers for the required record
    3) Open the PF PageHandle for the page
    4) Get the data from the page
    5) Calculate the record offset using the slot number
    6) Copy the record from the page to rec
        - Create a new record
        - Copy the data to the new record
        - Set the RID of the new record
        - Point rec to the new record
    7) Unpin the page
*/
RC RM_FileHandle::GetRec(const RID &rid, RM_Record &rec) const {
    // Check if the file is open
    if (!isOpen) {
        return RM_FILE_CLOSED;
    }

    // Delete the record if it is already valid
    if (rec.isValid) {
        rec.isValid = FALSE;
        delete[] rec.pData;
    }

    // Declare an integer for the return code
    int rc;

    // Get the page number for the required record
    PageNum pageNumber;
    if ((rc = rid.GetPageNum(pageNumber))) {
        // Return the error from the RID class
        return rc;
    }

    // Get the slot number for the required record
    SlotNum slotNumber;
    if ((rc = rid.GetSlotNum(slotNumber))) {
        // Return the error from the RID class
        return rc;
    }

    // Check whether the page number is valid
    if (pageNumber <= 0) {
        // Return error
        return RM_INVALID_PAGE_NUMBER;
    }

    // Open the corresponding PF page handle
    PF_PageHandle pfPH;
    if ((rc = pfFH.GetThisPage(pageNumber, pfPH))) {
        // Return the error from the PF FileHandle
        return rc;
    }

    // Get the data from the page
    char* pData;
    if ((rc = pfPH.GetData(pData))) {
        // Return the error from the PF PageHandle
        return rc;
    }

    // Check whether the slot number is valid
    int numberRecords = fileHeader.numberRecordsOnPage;
    if (slotNumber < 1 || slotNumber > numberRecords) {
        // Return error
        return RM_INVALID_SLOT_NUMBER;
    }

    // Get the record offset
    int recordOffset = getRecordOffset(slotNumber);

    // Set valid flag of record to true
    rec.isValid = TRUE;

    // Set the data in the new record
    int recordSize = fileHeader.recordSize;
    char* data = pData + recordOffset;
    char* newPData = new char[recordSize];
    memcpy(newPData, data, recordSize);
    rec.pData = newPData;

    // Set the RID and size of the new record
    rec.rid = rid;
    rec.recordSize = recordSize;

    // Unpin the page
    if ((rc = pfFH.UnpinPage(pageNumber))) {
        // Return the error from the PF FileHandle
        return rc;
    }

    // Return OK
    return OK_RC;
}


// Method: InsertRec(const char *pData, RID &rid)
// Insert a new record
/* Steps:
    1) Check if the file is open
    2) Get the first free page from the file header
    3) If no free page (first free page number is RM_NO_FREE_PAGE)
        - Allocate a new page
        - Initialize the page header and bitmap
        - Increment the number of pages in the file header
        - Update the first free page number in the header
    4) Mark the page as dirty
    5) Get the first free slot from the bitmap
    6) Calculate the record offset
    7) Insert the record in the free slot
        - Copy the data to the free slot
        - Update the bitmap on the page
    8) If the page becomes full (check bitmap)
        - Get the next free page number
        - Update the first free page number in the file header
        - Set the next free page on the page header to RM_NO_FREE_PAGE
    9) Unpin the page
    10) Set the rid to this record
*/
RC RM_FileHandle::InsertRec(const char *pData, RID &rid) {
    // Check if the file is open
    if (!isOpen) {
        return RM_FILE_CLOSED;
    }

    // Check that pData is not null
    if (pData == NULL) {
        return RM_NULL_RECORD;
    }

    // Declare an integer for the return code
    int rc;

    // Calculate the bitmap size
    int numberRecords = fileHeader.numberRecordsOnPage;
    int bitmapSize = numberRecords/8;
    if (numberRecords%8 != 0) bitmapSize++;

    // Get the first free page number
    PageNum freePageNumber = fileHeader.firstFreePage;
    PF_PageHandle pfPH;

    // If there is no free page
    if (freePageNumber == RM_NO_FREE_PAGE) {
        // Allocate a new page in the file
        if ((rc = pfFH.AllocatePage(pfPH))) {
            // Return the error from the PF FileHandle
            return rc;
        }

        // Set the page header and bitmap for the new page
        char* pHData;
        if ((rc = pfPH.GetData(pHData))) {
            // Return the error from the PF PageHandle
            return rc;
        }

        // Initialize the page header
        RM_PageHeader* pageHeader = new RM_PageHeader;
        pageHeader->nextPage = RM_NO_FREE_PAGE;

        // Initialize the bitmap to all 0s
        char* bitmap = new char[bitmapSize];
        for (int i=0; i<bitmapSize; i++) {
            bitmap[i] = 0x00;
        }

        // Copy the page header and bitmap to pHData
        memcpy(pHData, pageHeader, sizeof(RM_PageHeader));
        char* offset = pHData + sizeof(RM_PageHeader);
        memcpy(offset, bitmap, bitmapSize);

        // Set freePageNumber to this page
        if ((rc = pfPH.GetPageNum(freePageNumber))) {
            // Return the error from the PF PageHandle
            return rc;
        }

        // Increment the number of pages in the file header
        fileHeader.numberPages++;
        headerModified = TRUE;

        // Update the first free page number in the header
        fileHeader.firstFreePage = freePageNumber;

        // Delete the page header and bitmap
        delete pageHeader;
        delete[] bitmap;

        // Unpin the allocated page
        if ((rc = pfFH.UnpinPage(freePageNumber))) {
            // Return the error from the PF FileHandle
            return rc;
        }
    }

    // Get the page data
    if ((rc = pfFH.GetThisPage(freePageNumber, pfPH))) {
        // Return the error from the PF FileHandle
        return rc;
    }
    char* freePageData;
    if ((rc = pfPH.GetData(freePageData))) {
        // Return the error from the PF PageHandle
        return rc;
    }

    // Get the first free slot from the bitmap
    char* bitmap = freePageData + sizeof(RM_PageHeader);
    int freeSlotNumber = getFirstZeroBit(bitmap, bitmapSize);
    if (freeSlotNumber == RM_INCONSISTENT_BITMAP) {
        return RM_INCONSISTENT_BITMAP;
    }

    // Mark the page as dirty
    if ((rc = pfFH.MarkDirty(freePageNumber))) {
        // Return the error from the PF FileHandle
        return rc;
    }

    // Calculate the record offset
    int recordOffset = getRecordOffset(freeSlotNumber);

    // Copy the data to the free slot
    memcpy(freePageData+recordOffset, pData, fileHeader.recordSize);

    // Update the bitmap
    if ((rc = SetBit(freeSlotNumber, bitmap))) {
        // Return the error from the set bit method
        return rc;
    }

    // Check if the page has become full
    if (isBitmapFull(bitmap, numberRecords)) {
        // Get the next free page number
        RM_PageHeader* pH = (RM_PageHeader*) freePageData;
        int nextFreePageNumber = pH->nextPage;

        // Update the first free page in the file header
        fileHeader.firstFreePage = nextFreePageNumber;
        headerModified = TRUE;

        // Set the next free page on the current page header
        pH->nextPage = RM_NO_FREE_PAGE;
    }

    // Unpin the page
    if ((rc = pfFH.UnpinPage(freePageNumber))) {
        // Return the error from the PF FileHandle
        return rc;
    }

    // Set the RID
    RID newRid(freePageNumber, freeSlotNumber);
    rid = newRid;

    // Return OK
    return OK_RC;
}


// Method: DeleteRec(const RID &rid)
// Delete a record
/* Steps:
    1) Check if the file is open
    2) Get the page number and slot number from the RID
        - Check if the slot number is valid
    3) Mark the page as dirty
    4) Check if the page is full (check bitmap)
    5) Change the bit in the bitmap to 0
    6) If the page was previously full
        - Set the next free page in the page header to the first free page in the file header
        - Set the first free page in the file header to this page
    7) Unpin the page
    8) If the page becomes empty (check bitmap)
        - Delete the bitmap
        - Dispose the page using the PF FileHandle
        - Decrement the number of pages in the file header
*/
RC RM_FileHandle::DeleteRec(const RID &rid) {
    // Check if the file is open
    if (!isOpen) {
        return RM_FILE_CLOSED;
    }

    // Declare an integer for the return code
    int rc;

    // Get the page number and slot number from the RID
    int pageNumber, slotNumber;
    if ((rc = rid.GetPageNum(pageNumber))) {
        // Return the error from the RID
        return rc;
    }
    if ((rc = rid.GetSlotNum(slotNumber))) {
        // Return the error from the RID
        return rc;
    }

    // Check whether the page number is valid
    if (pageNumber <= 0) {
        // Return error
        return RM_INVALID_PAGE_NUMBER;
    }

    // Check whether the slot number is valid
    int numberRecords = fileHeader.numberRecordsOnPage;
    if (slotNumber < 1 || slotNumber > numberRecords) {
        // Return error
        return RM_INVALID_SLOT_NUMBER;
    }

    // Get the page data
    PF_PageHandle pfPH;
    char* pageData;
    if ((rc = pfFH.GetThisPage(pageNumber, pfPH))) {
        // Return the error from the PF FileHandle
        return rc;
    }
    if ((rc = pfPH.GetData(pageData))) {
        // Return the error from the PF PageHandle
        return rc;
    }

    // Mark the page as dirty
    if ((rc = pfFH.MarkDirty(pageNumber))) {
        // Return the error from the PF FileHandle
        return rc;
    }

    // Check if the page is full
    int bitmapSize = numberRecords/8;
    if (numberRecords%8 != 0) bitmapSize++;
    char* bitmap = pageData + sizeof(RM_PageHeader);
    bool pageFull = isBitmapFull(bitmap, numberRecords);

    // Change the corresponding bit in the bitmap to 0
    if ((rc = UnsetBit(slotNumber, bitmap))) {
        // Return the error from the unset bit method
        return rc;
    }

    // If the page was previously full, add it now to the free list
    if (pageFull) {
        // Get the original first free page
        PageNum firstFreePage = fileHeader.firstFreePage;

        // Set the next free page in the page header to this
        RM_PageHeader* pH = (RM_PageHeader*) pageData;
        pH->nextPage = firstFreePage;

        // Set the first free page in the file header to this page
        fileHeader.firstFreePage = pageNumber;
        headerModified = TRUE;
    }

    // Unpin the page
    if ((rc = pfFH.UnpinPage(pageNumber))) {
        // Return the error from the PF FileHandle
        return rc;
    }

    // TODO
    // Cannot dispose a page directly since the free list will be broken
    // Right now, keep the empty page in the file

    /*
    // If the page becomes empty, dispose the page
    if (isBitmapEmpty(bitmap, numberRecords)) {
        // Delete the bitmap
        delete[] bitmap;

        // Dispose the page using the PF FileHandle
        if ((rc = pfFH.DisposePage(pageNumber))) {
            // Return the error from the PF FileHandle
            return rc;
        }

        // Decrement the number of pages in the file header
        fileHeader.numberPages--;
        headerModified = TRUE;
    }
    */

    // Return OK
    return OK_RC;
}


// Method: UpdateRec(const RM_Record &rec)
// Update a record
/* Steps:
    1) Check if the file is open
    2) Get the RID for the record
    3) Get the page number and slot number for the record
        - Check whether the slot number is valid
    4) Mark the page as dirty
    5) Open the PF PageHandle for the page
    6) Calculate the record offset
    7) Update the record on the page
        - Copy the data from the record to the data on page
    8) Unpin the page
*/
RC RM_FileHandle::UpdateRec(const RM_Record &rec) {
    // Check if the file is open
    if (!isOpen) {
        return RM_FILE_CLOSED;
    }

    // Declare an integer for the return code
    int rc;

    // Get the RID for the record
    RID rid;
    if ((rc = rec.GetRid(rid))) {
        // Return the error from the RM Record
        return rc;
    }

    // Get the page number for the required record
    PageNum pageNumber;
    if ((rc = rid.GetPageNum(pageNumber))) {
        // Return the error from the RID class
        return rc;
    }

    // Get the slot number for the required record
    SlotNum slotNumber;
    if ((rc = rid.GetSlotNum(slotNumber))) {
        // Return the error from the RID class
        return rc;
    }

    // Check whether the page number is valid
    if (pageNumber <= 0) {
        // Return error
        return RM_INVALID_PAGE_NUMBER;
    }

    // Check whether the slot number is valid
    int numberRecords = fileHeader.numberRecordsOnPage;
    if (slotNumber < 1 || slotNumber > numberRecords) {
        // Return an error
        return RM_INVALID_SLOT_NUMBER;
    }

    // Open the corresponding PF page handle
    PF_PageHandle pfPH;
    if ((rc = pfFH.GetThisPage(pageNumber, pfPH))) {
        // Return the error from the PF FileHandle
        return rc;
    }

    // Get the data from the page
    char* pData;
    if ((rc = pfPH.GetData(pData))) {
        // Return the error from the PF PageHandle
        return rc;
    }

    // Mark the page as dirty
    if ((rc = pfFH.MarkDirty(pageNumber))) {
        // Return the error from the PF FileHandle
        return rc;
    }

    // Get the record offset
    int recordOffset = getRecordOffset(slotNumber);
    char* recordData = pData + recordOffset;

    // Get the record data
    char* recData;
    if ((rc = rec.GetData(recData))) {
        // Return the error from the RM Record
        return rc;
    }

    // Update the record in the file to the record data
    memcpy(recordData, recData, fileHeader.recordSize);

    // Unpin the page
    if ((rc = pfFH.UnpinPage(pageNumber))) {
        // Return the error from the PF FileHandle
        return rc;
    }

    // Return OK
    return OK_RC;
}


// Method: ForcePages(PageNum pageNum = ALL_PAGES)
// Forces a page (along with any contents stored in this class)
// from the buffer pool to disk.  Default value forces all pages.
RC RM_FileHandle::ForcePages(PageNum pageNum) {
    // Declare an integer for the return code
    int rc;

    // Force the pages using the PF FileHandle
    if ((rc = pfFH.ForcePages(pageNum))) {
        // Return the error from the PF FileHandle
        return rc;
    }

    // Return OK
    return OK_RC;
}


// Method: getRecordOffset(int slotNumber)
// Calculate the record offset given the slot number
int RM_FileHandle::getRecordOffset(int slotNumber) const {
    // Get the bitmap size
    int numberRecords = fileHeader.numberRecordsOnPage;
    int bitmapSize = numberRecords/8;
    if (numberRecords % 8 != 0) bitmapSize++;

    // Get the record offset
    int recordSize = fileHeader.recordSize;
    int recordOffset = sizeof(RM_PageHeader) + bitmapSize + (slotNumber-1)*recordSize;

    return recordOffset;
}

// Method: SetBit(int bitNumer, char* bitmap)
// Set bit in the bitmap to 1
RC RM_FileHandle::SetBit(int bitNumber, char* bitmap) {
    // Change bit number to start from 0
    bitNumber--;

    // Calculate the byte number (start from 0)
    int byteNumber = bitNumber/8;
    char currentByte = bitmap[byteNumber];

    // Calculate the bit offset in the current byte
    int bitOffset = bitNumber - (byteNumber*8);

    // Return error if the bit is not 0
    if ((currentByte | (0x80 >> bitOffset)) == currentByte) {
        return RM_INCONSISTENT_BITMAP;
    }

    // Set the bit to 1
    currentByte |= (0x80 >> bitOffset);
    bitmap[byteNumber] = currentByte;

    // Return OK
    return OK_RC;
}

// Method: UnsetBit(int bitNumber, char* bitmap)
// Set bit in the bitmap to 0
RC RM_FileHandle::UnsetBit(int bitNumber, char* bitmap) {
    // Change bit number to start from 0
    bitNumber--;

    // Calculate the byte number (start from 0)
    int byteNumber = bitNumber/8;
    char currentByte = bitmap[byteNumber];

    // Calculate the bit offset in the current byte
    int bitOffset = bitNumber - (byteNumber*8);

    // Return error if the bit is not 1
    if ((currentByte | (0x80 >> bitOffset)) != currentByte) {
        return RM_INCONSISTENT_BITMAP;
    }

    // Set the bit to 0
    currentByte &= ~(0x80 >> bitOffset);
    bitmap[byteNumber] = currentByte;

    // Return OK
    return OK_RC;
}

// Method: int getFirstZeroBit(char* bitmap, int bitmapSize)
// Get the first 0 bit in the bitmap
int RM_FileHandle::getFirstZeroBit(char* bitmap, int bitmapSize) {
    // Iterate over the size of the bitmap
    for (int i=0; i<bitmapSize; i++) {
        char currentByte = bitmap[i];
        // Check the 8 bits of the current byte
        for (int j=0; j<8; j++) {
            // Check if the first bit of the shifted byte is 0
            if ((currentByte | (0x80 >> j)) != currentByte) {
                return i*8 + j + 1;
            }
        }
    }

    // Return error
    return RM_INCONSISTENT_BITMAP;
}

// Method: bool isBitmapFull(char* bitmap, int numberRecords)
// Check if the bitmap is all 1s
bool RM_FileHandle::isBitmapFull(char* bitmap, int numberRecords) {
    int count = 0;
    int bitNumber = 0;
    int byteNumber = 0;
    char currentByte = bitmap[byteNumber];

    // Iterate over the size of the bitmap
    while (count < numberRecords) {
        // Check if the current bit is not 1
        if ((currentByte | (0x80 >> bitNumber)) != currentByte) {
            return false;
        }
        count++;
        bitNumber++;
        if (bitNumber == 8) {
            byteNumber++;
            bitNumber = 0;
            currentByte = bitmap[byteNumber];
        }
    }
    return true;
}

// Method: bool isBitmapEmpty(char* bitmap, int numberRecords)
// Check if the bitmap is all 0s
bool RM_FileHandle::isBitmapEmpty(char* bitmap, int numberRecords) {
    int count = 0;
    int bitNumber = 0;
    int byteNumber = 0;
    char currentByte = bitmap[byteNumber];

    // Iterate over the size of the bitmap
    while (count < numberRecords) {
        // Check if the current bit is not 0
        if ((currentByte | (0x80 >> bitNumber)) == currentByte) {
            return false;
        }
        count++;
        bitNumber++;
        if (bitNumber == 8) {
            byteNumber++;
            bitNumber = 0;
            currentByte = bitmap[byteNumber];
        }
    }
    return true;
}