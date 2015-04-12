//
// File:        rm_rid.cc
// Description: RID class implementation
// Authors:     Aditya Bhandari (adityasb@stanford.edu)
//

#include "rm_internal.h"
#include "rm_rid.h"
using namespace std;

// Default constructor
RID::RID() {
    // Set the viable flag to false
    this->isViable = FALSE;
}

// Constructor with PageNum and SlotNum given
RID::RID(PageNum pageNum, SlotNum slotNum) {
    // Copy the page number and slot number into the local variables
    this->pageNumber = pageNum;
    this->slotNumber = slotNum;

    // Set the viable flag to true
    this->isViable = TRUE;
}

// Destructor
RID::~RID() {
    // Nothing to free
}

// Copy constructor
RID::RID(const RID &rid) {
    // Copy the page number, slot number and viable flag
    this->pageNumber = rid.pageNumber;
    this->slotNumber = rid.slotNumber;
    this->isViable = rid.isViable;
}

// Overload =
RID& RID::operator=(const RID &rid) {
    // Check for self-assignment
    if (this != &rid) {
        // Copy the page number, slot number and viable flag
        this->pageNumber = rid.pageNumber;
        this->slotNumber = rid.slotNumber;
        this->isViable = rid.isViable;
    }

    // Return a reference to this
    return (*this);
}

// Method: GetPageNum(PageNum &pageNum) const
// Return page number
RC RID::GetPageNum(PageNum &pageNum) const {
    // If the RID is not viable, return a positive error
    if (!isViable) {
        return RID_NOT_VIABLE;
    }
    else {
        // Set pageNum to the page number of the RID
        pageNum = this->pageNumber;

        // Return OK
        return OK_RC;
    }
}

// Method: GetSlotNum(SlotNum &slotNum) const
// Return slot number
RC RID::GetSlotNum(SlotNum &slotNum) const {
    // If the RID is not viable, return a positive error
    if (!isViable) {
        return RID_NOT_VIABLE;
    }
    else {
        // Set slotNum to the slot number of the RID
        slotNum = this->slotNumber;

        // Return OK
        return OK_RC;
    }
}