//
// File:        ix_manager.cc
// Description: IX_Manager class implementation
// Authors:     Aditya Bhandari (adityasb@stanford.edu)
//

#include "ix_internal.h"
#include "ix.h"
using namespace std;

// Constructor
IX_Manager::IX_Manager(PF_Manager &pfm) {
    // Store the PF_Manager
    this->pfManager = &pfm;
}

// Destructor
IX_Manager::~IX_Manager() {
    // Nothing to free
}

// Method: CreateIndex(const char *fileName, int indexNo, AttrType attrType, int attrLength)
// Create a new Index for the given file name
/* Steps:

*/
RC IX_Manager::CreateIndex(const char *fileName, int indexNo,
                           AttrType attrType, int attrLength) {

}

// Method: DestroyIndex(const char *fileName, int indexNo)
// Destroy an Index
/* Steps:

*/
RC IX_Manager::DestroyIndex(const char *fileName, int indexNo) {

}

// Method: OpenIndex(const char *fileName, int indexNo, IX_IndexHandle &indexHandle)
// Open an Index
/* Steps:

*/
RC IX_Manager::OpenIndex(const char *fileName, int indexNo, IX_IndexHandle &indexHandle) {

}

// Method: CloseIndex(IX_IndexHandle &indexHandle)
// Close an Index
/* Steps:

*/
RC IX_Manager::CloseIndex(IX_IndexHandle &indexHandle) {

}
