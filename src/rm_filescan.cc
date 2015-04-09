//
// File:        rm_filescan.cc
// Description: RM_FileScan class implementation
// Authors:     Aditya Bhandari (adityasb@stanford.edu)
//

#include "rm.h"
using namespace std;

// Default constructor
RM_FileScan::RM_FileScan() {

}

// Destructor
RM_FileScan::~RM_FileScan() {

}

// Initialize a file scan
RC RM_FileScan::OpenScan(const RM_FileHandle &fileHandle, AttrType attrType, int attrLength,
                         int attrOffset, CompOp compOp, void *value, ClientHint pinHint = NO_HINT) {

}

// Get next matching record
RC RM_FileScan::GetNextRec(RM_Record &rec) {

}

// Close the scan
RC RM_FileScan::CloseScan() {

}