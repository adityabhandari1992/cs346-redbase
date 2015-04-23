//
// rm.h
//
//   Record Manager component interface
//
// This file does not include the interface for the RID class.  This is
// found in rm_rid.h
//

#ifndef RM_H
#define RM_H

// Please DO NOT include any files other than redbase.h and pf.h in this
// file.  When you submit your code, the test program will be compiled
// with your rm.h and your redbase.h, along with the standard pf.h that
// was given to you.  Your rm.h, your redbase.h, and the standard pf.h
// should therefore be self-contained (i.e., should not depend upon
// declarations in any other file).

// Do not change the following includes
#include "redbase.h"
#include "rm_rid.h"
#include "pf.h"


// RM_FileHeaderPage: Struct for the file header page
/* Stores the following:
    1) Record size - integer
    2) Number of records on a page - integer
    3) Number of pages on file - integer
    4) First free page - PageNum
*/
struct RM_FileHeaderPage {
    int recordSize;
    int numberRecordsOnPage;
    int numberPages;
    PageNum firstFreePage;
};

//
// RM_Record: RM Record interface
//
class RM_Record {
    friend class RM_FileHandle;
    friend class RM_FileScan;
public:
    RM_Record ();
    ~RM_Record();

    // Copy constructor
    RM_Record(const RM_Record &rec);

    // Overload =
    RM_Record& operator=(const RM_Record &rec);

    // Return the data corresponding to the record.  Sets *pData to the
    // record contents.
    RC GetData(char *&pData) const;

    // Return the RID associated with the record
    RC GetRid (RID &rid) const;

private:
    char* pData;        // Data in the record
    RID rid;            // RID of the record
    int isValid;        // Flag to store validity of record
    int recordSize;     // The record size
};

//
// RM_FileHandle: RM File interface
//
class RM_FileHandle {
    friend class RM_Manager;
    friend class RM_FileScan;
public:
    RM_FileHandle ();
    ~RM_FileHandle();

   // Copy constructor
   RM_FileHandle(const RM_FileHandle &fileHandle);

   // Overload =
   RM_FileHandle& operator=(const RM_FileHandle &fileHandle);

    // Given a RID, return the record
    RC GetRec     (const RID &rid, RM_Record &rec) const;

    RC InsertRec  (const char *pData, RID &rid);       // Insert a new record

    RC DeleteRec  (const RID &rid);                    // Delete a record
    RC UpdateRec  (const RM_Record &rec);              // Update a record

    // Forces a page (along with any contents stored in this class)
    // from the buffer pool to disk.  Default value forces all pages.
    RC ForcePages (PageNum pageNum = ALL_PAGES);

private:
    PF_FileHandle pfFH;             // PF file handle
    int isOpen;                     // File handle open flag
    int headerModified;             // Modified flag for the file header
    RM_FileHeaderPage fileHeader;   // File header information

    int getRecordOffset(int slotNumber) const;              // Get the record offset from slot number
    RC SetBit(int bitNumber, char* bitmap);                 // Set bit in the bitmap to 1
    RC UnsetBit(int bitNumber, char* bitmap);               // Set bit in the bitmap to 0
    int getFirstZeroBit(char* bitmap, int bitmapSize);      // Get the first 0 bit in the bitmap
    bool isBitmapFull(char* bitmap, int numberRecords);     // Check if the bitmap is all 1s
    bool isBitmapEmpty(char* bitmap, int numberRecords);    // Check if the bitmap is all 0s
};

//
// RM_FileScan: condition-based scan of records in the file
//
class RM_FileScan {
public:
    RM_FileScan  ();
    ~RM_FileScan ();

    RC OpenScan  (const RM_FileHandle &fileHandle,
                  AttrType   attrType,
                  int        attrLength,
                  int        attrOffset,
                  CompOp     compOp,
                  void       *value,
                  ClientHint pinHint = NO_HINT); // Initialize a file scan
    RC GetNextRec(RM_Record &rec);               // Get next matching record
    RC CloseScan ();                             // Close the scan

private:
    PageNum pageNumber;                                 // Current page number
    SlotNum slotNumber;                                 // Current slot number
    RM_FileHandle fileHandle;                           // File handle for the file
    AttrType attrType;                                  // Attribute type
    int attrLength;                                     // Attribute length
    int attrOffset;                                     // Attribute offset
    CompOp compOp;                                      // Comparison operator
    void* value;                                        // Value to be compared
    ClientHint pinHint;                                 // Pinning hint
    int scanOpen;                                       // Flag to track is scan is open

    int getIntegerValue(char* recordData);              // Get integer attribute value
    float getFloatValue(char* recordData);              // Get float attribute value
    std::string getStringValue(char* recordData);       // Get string attribute value
    bool isBitFilled(int bitNumber, char* bitmap);      // Check whether a slot is filled

    template<typename T>
    bool matchRecord(T recordValue, T givenValue);      // Match the record value with
                                                        // the given value
};

//
// RM_Manager: provides RM file management
//
class RM_Manager {
public:
    RM_Manager    (PF_Manager &pfm);
    ~RM_Manager   ();

    RC CreateFile (const char *fileName, int recordSize);
    RC DestroyFile(const char *fileName);
    RC OpenFile   (const char *fileName, RM_FileHandle &fileHandle);

    RC CloseFile  (RM_FileHandle &fileHandle);

private:
    PF_Manager* pfManager;                   // PF_Manager object
    int findNumberRecords(int recordSize);
};

//
// Print-error function
//
void RM_PrintError(RC rc);

// Warnings
#define RM_LARGE_RECORD             (START_RM_WARN + 0) // Record size is too large
#define RM_SMALL_RECORD             (START_RM_WARN + 1) // Record size is too small
#define RM_FILE_OPEN                (START_RM_WARN + 2) // File is already open
#define RM_FILE_CLOSED              (START_RM_WARN + 3) // File is closed
#define RM_RECORD_NOT_VALID         (START_RM_WARN + 4) // Record is not valid
#define RM_INVALID_SLOT_NUMBER      (START_RM_WARN + 5) // Slot number is not valid
#define RM_INVALID_PAGE_NUMBER      (START_RM_WARN + 6) // Page number is not valid
#define RM_ATTRIBUTE_NOT_CONSISTENT (START_RM_WARN + 7) // Attribute is not consistent
#define RM_SCAN_CLOSED              (START_RM_WARN + 8) // Scan is not open
#define RM_INVALID_FILENAME         (START_RM_WARN + 9) // Invalid filename
#define RM_INVALID_ATTRIBUTE        (START_RM_WARN + 10) // Invalid attribute
#define RM_INVALID_OFFSET           (START_RM_WARN + 11) // Invalid offset
#define RM_INVALID_OPERATOR         (START_RM_WARN + 12) // Invalid operator
#define RM_NULL_RECORD              (START_RM_WARN + 13) // Null record
#define RM_EOF                      (START_RM_WARN + 14) // End of file
#define RM_LASTWARN                 RM_EOF

// Errors
#define RM_INVALIDNAME          (START_RM_ERR - 0) // Invalid PC file name
#define RM_INCONSISTENT_BITMAP  (START_RM_ERR - 1) // Inconsistent bitmap in page

// Error in UNIX system call or library routine
#define RM_UNIX            (START_RM_ERR - 2) // Unix error
#define RM_LASTERROR       RM_UNIX

#endif
