//
// File:        ix_indexscan.cc
// Description: IX_IndexScan class implementation
// Authors:     Aditya Bhandari (adityasb@stanford.edu)
//

#include "ix_internal.h"
#include "ix.h"
using namespace std;

// Constructor
IX_IndexScan::IX_IndexScan() {

}

// Destructor
IX_IndexScan::~IX_IndexScan() {

}

// Method: OpenScan(const IX_IndexHandle &indexHandle, CompOp compOp,
//                  void *value, ClientHint  pinHint)
// Open index scan
/* Steps:

*/
RC IX_IndexScan::OpenScan(const IX_IndexHandle &indexHandle, CompOp compOp,
                          void *value, ClientHint  pinHint) {

}

// Method: GetNextEntry(RID &rid)
// Get the next matching entry
// Return IX_EOF if no more matching entries
/* Steps:

*/
RC IX_IndexScan::GetNextEntry(RID &rid) {

}

// Method: CloseScan()
// Close index scan
/* Steps:

*/
RC IX_IndexScan::CloseScan() {

}
