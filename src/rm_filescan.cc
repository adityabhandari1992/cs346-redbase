//
// File:        rm_filescan.cc
// Description: RM_FileScan class implementation
// Authors:     Aditya Bhandari (adityasb@stanford.edu)
//

#include <iostream>
#include <cstring>
#include <string>
#include "rm.h"
using namespace std;

// Default constructor
RM_FileScan::RM_FileScan() {
    // Set open scan flag to false
    scanOpen = FALSE;
}

// Destructor
RM_FileScan::~RM_FileScan() {
    // Delete the file handle
    delete &fileHandle;
}

// Method: OpenScan(const RM_FileHandle &fileHandle, AttrType attrType, int attrLength,
//                  int attrOffset, CompOp compOp, void *value, ClientHint pinHint = NO_HINT)
// Initialize a file scan
/* Steps:
    1) Initialize the class variables
        - Store attrType, attrLength, attrOffset, compOp, value and pinHint
        - Store the page number and slot number of the first (non-header) page of the file
*/
RC RM_FileScan::OpenScan(const RM_FileHandle &fileHandle, AttrType attrType, int attrLength,
                         int attrOffset, CompOp compOp, void *value, ClientHint pinHint) {
    // Check for erroneous input
    if ((attrType == INT || attrType == FLOAT) && attrLength != 4) {
        return RM_ATTRIBUTE_NOT_CONSISTENT;
    }
    if (attrType == STRING) {
        if (attrLength < 1 || attrLength > MAXSTRINGLEN) {
            return RM_ATTRIBUTE_NOT_CONSISTENT;
        }
    }
    if (compOp == NO_OP && value != NULL) {
        return RM_ATTRIBUTE_NOT_CONSISTENT;
    }

    // Store the class variables
    this->fileHandle = fileHandle;
    this->attrType = attrType;
    this->attrLength = attrLength;
    this->attrOffset = attrOffset;
    this->compOp = compOp;
    this->value = value;
    this->pinHint = pinHint;

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
    if ((rc = pfFH.GetNextPage(headerPageNumber, pfPH))) {
        // Return the error from the PF FileHandle
        return rc;
    }
    if ((rc = pfPH.GetPageNum(pageNumber))) {
        // Return the error from the PF PageHandle
        return rc;
    }

    // Set the page and slot numbers
    this->pageNumber = pageNumber;
    this->slotNumber = 1;

    // Set the scan open flag
    scanOpen = TRUE;

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
    8) Follow the pin hint - TODO later
    9) If next record was not found, go to (2)
*/
RC RM_FileScan::GetNextRec(RM_Record &rec) {
    // Return error if the scan is closed
    if (!scanOpen) {
        return RM_SCAN_CLOSED;
    }

    // Declare an integer for the return code
    int rc;

    // Declare required variables
    PF_FileHandle pfFH = fileHandle.pfFH;
    PF_PageHandle pfPH;
    char* pageData;
    char* bitmap;

    // Get the page corresponding to the page number
    if ((rc = pfFH.GetThisPage(pageNumber, pfPH))) {
        // Return the error from the PF FileHandle
        return rc;
    }
    // Get the bitmap on the page
    if ((rc = pfPH.GetData(pageData))) {
        // Return the error from the PF PageHandle
        return rc;
    }
    bitmap = pageData + sizeof(RM_PageHeader);

    // Do while next record is not found
    bool recordFound = false;
    while(!recordFound) {
        // Check whether the slot in the bitmap is filled
        if (isBitFilled(slotNumber, bitmap)) {
            // Get the record data from the page
            int recordOffset = fileHandle.getRecordOffset(slotNumber);
            char* recordData = pageData + recordOffset;

            // Get the required attribute and compare
            bool recordMatch = false;

            // If the attribute is integer
            if (attrType == INT) {
                int recordValue = getIntegerValue(recordData);
                int givenValue = *static_cast<int*>(value);

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
                    case NO_OP:
                        recordMatch = true;
                        break;
                }
            }
            // If the attribute is float
            else if (attrType == FLOAT) {
                float recordValue = getFloatValue(recordData);
                float givenValue = *static_cast<float*>(value);

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
                    case NO_OP:
                        recordMatch = true;
                        break;
                }
            }
            // If the attribute is string
            else if (attrType == STRING) {
                string recordValue = getStringValue(recordData);
                char* givenValueChar = static_cast<char*>(value);
                string givenValue = "";
                for (int i=0; i<attrLength; i++) {
                    givenValue += givenValueChar[i];
                }

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
                    case NO_OP:
                        recordMatch = true;
                        break;
                }
            }

            // If the record matches
            if (recordMatch) {
                // Create a new record
                RM_Record newRecord;

                // Set the data in the new record
                char* newPData;
                if ((rc = newRecord.GetData(newPData))) {
                    // Return the error from the RM Record
                    return rc;
                }
                memcpy(newPData, recordData, (fileHandle.fileHeader).recordSize);

                // Set the RID of the new record
                RID newRid;
                if ((rc = newRecord.GetRid(newRid))) {
                    // Return the error from the RM Record
                    return rc;
                }

                RID rid(pageNumber, slotNumber);
                newRid = rid;

                // Set valid flag of record to true
                newRecord.isValid = TRUE;

                // Set rec to point to the new record
                rec = newRecord;
            }
        }

        // Increment the slot number
        // Check if this is the last slot
        if (slotNumber == (fileHandle.fileHeader).numberRecordsOnPage) {
            // Get the next page of the file
            rc = pfFH.GetNextPage(pageNumber, pfPH);
            if (rc == PF_EOF) {
                // Return EOF
                return RM_EOF;
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

    // Follow the pin hint - TODO
    if (pinHint == NO_HINT) {
        // Do nothing
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
    memcpy(&attrPointer, recordData, sizeof(recordValue));
    return recordValue;
}

// Method: float getFloatValue(char* recordData)
// Get float attribute value
float RM_FileScan::getFloatValue(char* recordData) {
    float recordValue;
    char* attrPointer = recordData + attrOffset;
    memcpy(&attrPointer, recordData, sizeof(recordValue));
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