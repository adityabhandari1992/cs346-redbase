//
// File:        rm_record.cc
// Description: RM_Record class implementation
// Authors:     Aditya Bhandari (adityasb@stanford.edu)
//

#include "rm.h"
#include "rm_rid.h"
using namespace std;

// Default constructor
RM_Record::RM_Record() {
    // Set the valid flag to false
    this->isValid = FALSE;
}

// Destructor
RM_Record::~RM_Record() {
    // Delete the RID
    delete &rid;
}

// Method: GetData(char *&pData) const
// Return the data corresponding to the record
RC RM_Record::GetData(char *&pData) const {
    // Check if the record is valid
    if (!isValid) {
        return RM_RECORD_NOT_VALID;
    }

    // Point pData to the data in the record
    pData = this->pData;

    // Return OK
    return OK_RC;
}

// Method: GetRid (RID &rid) const
// Return the RID associated with the record
RC RM_Record::GetRid (RID &rid) const {
    // Check if the record is valid
    if (!isValid) {
        return RM_RECORD_NOT_VALID;
    }

    // Point rid to the RID in the record
    rid = this->rid;

    // Return OK
    return OK_RC;
}