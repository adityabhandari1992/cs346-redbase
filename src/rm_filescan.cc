//
// File:        rm_filescan.cc
// Description: RM_FileScan class implementation
// Authors:     Aditya Bhandari (adityasb@stanford.edu)
//

#include <iostream>
#include <cstring>
#include <string>
#include "rm_internal.h"
#include "rm.h"
using namespace std;

// Default constructor
RM_FileScan::RM_FileScan() {
    // Set open scan flag to false
    scanOpen = FALSE;
}

// Destructor
RM_FileScan::~RM_FileScan() {
    // Nothing to free
}

// Method: OpenScan(const RM_FileHandle &fileHandle, AttrType attrType, int attrLength,
//                  int attrOffset, CompOp compOp, void *value, ClientHint pinHint = NO_HINT)
// Initialize a file scan
/* Steps:
    1) Initialize the class variables
        - Store attrType, attrLength, attrOffset, compOp, value and pinHint
        - Store the page number and slot number of the first (non-header) page of the file
    2) Unpin the header and data pages
*/
RC RM_FileScan::OpenScan(const RM_FileHandle &fileHandle, AttrType attrType, int attrLength,
                         int attrOffset, CompOp compOp, void *value, ClientHint pinHint) {
    // Check for erroneous input
    if (attrType != INT && attrType != FLOAT && attrType != STRING) {
        return RM_INVALID_ATTRIBUTE;
    }

    if (!fileHandle.isOpen) {
        return RM_FILE_CLOSED;
    }

    int recordSize = (fileHandle.fileHeader).recordSize;
    if (attrOffset > recordSize || attrOffset < 0) {
        return RM_INVALID_OFFSET;
    }

    if (compOp != NO_OP && compOp != EQ_OP && compOp != NE_OP && compOp != LT_OP &&
        compOp != GT_OP && compOp != LE_OP && compOp != GE_OP) {
        return RM_INVALID_OPERATOR;
    }

    if ((attrType == INT || attrType == FLOAT) && attrLength != 4) {
        return RM_ATTRIBUTE_NOT_CONSISTENT;
    }
    if (attrType == STRING) {
        if (attrLength < 1 || attrLength > MAXSTRINGLEN) {
            return RM_ATTRIBUTE_NOT_CONSISTENT;
        }
    }

    // If the value is a null pointer, set compOp to NO_OP
    if (compOp != NO_OP && value == NULL) {
        compOp = NO_OP;
    }

    // Store the class variables
    this->fileHandle = fileHandle;
    this->attrType = attrType;
    this->attrLength = attrLength;
    this->attrOffset = attrOffset;
    this->compOp = compOp;
    this->value = value;
    this->pinHint = pinHint;

    // Set the scan open flag
    scanOpen = TRUE;

    // Declare an integer for return code
    int rc;

    // Get the page number of the first page (header)
    PF_FileHandle pfFH = fileHandle.pfFH;
    PF_PageHandle pfPH;
    if ((rc = pfFH.GetFirstPage(pfPH))) {
        // Return the error from the PF FileHandle
        return rc;
    }

    PageNum headerPageNumber;
    if ((rc = pfPH.GetPageNum(headerPageNumber))) {
        // Return the error from the PF PageHandle
        return rc;
    }

    // Get the page number of the first data page
    PageNum pageNumber;
    bool pageFound = true;
    if ((rc = pfFH.GetNextPage(headerPageNumber, pfPH))) {
        if (rc == PF_EOF) {
            pageNumber = RM_NO_FREE_PAGE;
            pageFound = false;
        }
        else {
            // Return the error from the PF FileHandle
            return rc;
        }
    }
    if (pageFound) {
        if ((rc = pfPH.GetPageNum(pageNumber))) {
            // Return the error from the PF PageHandle
            return rc;
        }
    }

    // Set the page and slot numbers
    this->pageNumber = pageNumber;
    this->slotNumber = 1;

    // Unpin the header and data page
    if ((rc = pfFH.UnpinPage(headerPageNumber))) {
        // Return the error from the PF FileHandle
        return rc;
    }
    if (pageFound) {
        if ((rc = pfFH.UnpinPage(pageNumber))) {
            // Return the error from the PF FileHandle
            return rc;
        }
    }

    // Return OK
    return OK_RC;
}

// Method: GetNextRec(RM_Record &rec)
// Get the next matching record
/* Steps:
    1) Get the page using the page number
    2) Check the slot number in the bitmap in the page
    3) If filled, get the record data from the page
    4) Get the required attribute at the given offset
    5) Compare the attribute with the given value
    6) If it satisfies the condition
        - Create a new record and fill its data and RID
        - Point rec to the new record
    7) Increment the slot number
        - If not the last slot, increment by 1
        - Else, get the next page of the file
            - If PF_EOF, return RM_EOF
            - Set the new page number
            - Set slot number to 1
    8) Follow the pin hint
    9) If next record was not found, go to (2)
*/
RC RM_FileScan::GetNextRec(RM_Record &rec) {
    // Return error if the scan is closed
    if (!scanOpen) {
        return RM_SCAN_CLOSED;
    }

    // Delete the record data if it is already valid
    if (rec.isValid) {
        rec.isValid = FALSE;
        delete[] rec.pData;
    }

    // Declare an integer for the return code
    int rc;

    // Declare required variables
    PF_FileHandle pfFH = fileHandle.pfFH;
    PF_PageHandle pfPH;
    char* pageData;
    char* bitmap;

    // If the file is empty
    if (pageNumber == RM_NO_FREE_PAGE) {
        return RM_EOF;
    }

    // Get the page corresponding to the page number
    if ((rc = pfFH.GetThisPage(pageNumber, pfPH))) {
        return rc;
    }
    // Get the bitmap on the page
    if ((rc = pfPH.GetData(pageData))) {
        // Return the error from the PF PageHandle
        return rc;
    }
    bitmap = pageData + sizeof(RM_PageHeader);

    // Do while next record is not found
    bool recordMatch = false;
    while(!recordMatch) {
        // Check whether the slot in the bitmap is filled
        if (isBitFilled(slotNumber, bitmap)) {
            // Get the record data from the page
            int recordOffset = fileHandle.getRecordOffset(slotNumber);
            char* recordData = pageData + recordOffset;

            // Check if the operator is NO_OP
            if (compOp == NO_OP) {
                recordMatch = true;
            }
            // If the attribute is integer
            else if (attrType == INT) {
                int recordValue = getIntegerValue(recordData);
                int givenValue = *static_cast<int*>(value);
                recordMatch = matchRecord(recordValue, givenValue);
            }
            // If the attribute is float
            else if (attrType == FLOAT) {
                float recordValue = getFloatValue(recordData);
                float givenValue = *static_cast<float*>(value);
                recordMatch = matchRecord(recordValue, givenValue);
            }
            // If the attribute is string
            else if (attrType == STRING) {
                string recordValue(recordData + attrOffset);
                char* givenValueChar = static_cast<char*>(value);
                string givenValue(givenValueChar);
                recordMatch = matchRecord(recordValue, givenValue);
            }

            // If the record matches
            if (recordMatch) {
                // Set valid flag of record to true
                rec.isValid = TRUE;

                // Set the data in the new record
                int recordSize = (fileHandle.fileHeader).recordSize;
                char* newPData = new char[recordSize];
                memcpy(newPData, recordData, recordSize);
                rec.pData = newPData;

                // Set the RID and size of the new record
                RID newRid(pageNumber, slotNumber);
                rec.rid = newRid;
                rec.recordSize = recordSize;
            }
        }

        // Increment the slot number
        // Check if this is the last slot
        if (slotNumber == (fileHandle.fileHeader).numberRecordsOnPage) {
            // Unpin the previous page
            if ((rc = pfFH.UnpinPage(pageNumber))) {
                // Return the error from the PF FileHandle
                return rc;
            }

            // Get the next page of the file
            rc = pfFH.GetNextPage(pageNumber, pfPH);
            if (rc == PF_EOF) {
                pageNumber = RM_NO_FREE_PAGE;

                // Return OK if record already found, else return EOF
                if (recordMatch) return OK_RC;
                else return RM_EOF;
            }
            else if (rc) {
                // Return the error from the PF FileHandle
                return rc;
            }

            // Set the new page number
            if ((rc = pfPH.GetPageNum(pageNumber))) {
                // Return the error from the PF PageHandle
                return rc;
            }

            // Set slot number to 1
            slotNumber = 1;

            // Set the new page data and bitmap
            if ((rc = pfPH.GetData(pageData))) {
                // Return the error from the PF PageHandle
                return rc;
            }
            bitmap = pageData + sizeof(RM_PageHeader);
        }
        else {
            slotNumber++;
        }
    }

    // If no hint is given, unpin immediately
    if (pinHint == NO_HINT) {
        // Unpin the page
        if ((rc = pfFH.UnpinPage(pageNumber))) {
            // Return the error from the PF FileHandle
            return rc;
        }
    }

    // Return OK
    return OK_RC;
}

// Method: CloseScan()
// Close the file scan
/* Steps:
    1) Return error if the scan is not open
    2) Update the scan open flag
*/
RC RM_FileScan::CloseScan() {
    // Return error if the scan is not open
    if (!scanOpen) {
        return RM_SCAN_CLOSED;
    }

    // Set open scan flag to false
    scanOpen = FALSE;

    // Return OK
    return OK_RC;
}


// Method: int getIntegerValue(char* recordData)
// Get integer attribute value
int RM_FileScan::getIntegerValue(char* recordData) {
    int recordValue;
    char* attrPointer = recordData + attrOffset;
    memcpy(&recordValue, attrPointer, sizeof(recordValue));
    return recordValue;
}

// Method: float getFloatValue(char* recordData)
// Get float attribute value
float RM_FileScan::getFloatValue(char* recordData) {
    float recordValue;
    char* attrPointer = recordData + attrOffset;
    memcpy(&recordValue, attrPointer, sizeof(recordValue));
    return recordValue;
}

// Method: char* getStringValue(char* recordData)
// Get string attribute value
string RM_FileScan::getStringValue(char* recordData) {
    string recordValue = "";
    char* attrPointer = recordData + attrOffset;
    for (int i=0; i<attrLength; i++) {
        recordValue += attrPointer[i];
    }
    return recordValue;
}

// Method: bool isBitFilled(int bitNumber, char* bitmap)
// Check whether a slot is filled
bool RM_FileScan::isBitFilled(int bitNumber, char* bitmap) {
    // Change bit number to start from 0
    bitNumber--;

    // Calculate the byte number (start from 0)
    int byteNumber = bitNumber/8;
    char currentByte = bitmap[byteNumber];

    // Calculate the bit offset in the current byte
    int bitOffset = bitNumber - (byteNumber*8);

    // Return whether the bit is 1
    return ((currentByte | (0x80 >> bitOffset)) == currentByte);
}

// Template method: matchRecord(T recordValue, T givenValue, CompOp compOp)
// Match the record value with the given value
template<typename T>
bool RM_FileScan::matchRecord(T recordValue, T givenValue) {
    bool recordMatch = false;
    switch(compOp) {
        case EQ_OP:
            if (recordValue == givenValue) recordMatch = true;
            break;
        case LT_OP:
            if (recordValue < givenValue) recordMatch = true;
            break;
        case GT_OP:
            if (recordValue > givenValue) recordMatch = true;
            break;
        case LE_OP:
            if (recordValue <= givenValue) recordMatch = true;
            break;
        case GE_OP:
            if (recordValue >= givenValue) recordMatch = true;
            break;
        case NE_OP:
            if (recordValue != givenValue) recordMatch = true;
            break;
        default:
            break;
    }
    return recordMatch;
}