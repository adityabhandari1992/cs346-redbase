//
// File:        sm_manager.cc
// Description: SM_Manager class implementation
// Authors:     Aditya Bhandari (adityasb@stanford.edu)
//

#include "sm.h"
#include <string>
#include <cstring>
using namespace std;

// Constructor
SM_Manager(IX_Manager &ixm, RM_Manager &rmm) {

}

// Destructor
~SM_Manager() {

}

// Open the database
RC OpenDb(const char *dbName) {

}

// Close the database
RC CloseDb() {

}

// Create relation relName, given number of attributes and attribute data
RC CreateTable(const char *relName, int attrCount, AttrInfo *attributes) {

}

// Create an index for relName.attrName
RC CreateIndex(const char *relName, const char *attrName) {

}

// Destroy a relation
RC DropTable(const char *relName) {

}

// Destroy index on relName.attrName
RC DropIndex(const char *relName, const char *attrName) {

}

// Load relName from fileName
RC Load(const char *relName, const char *fileName) {

}

// Print relations in db
RC Help() {

}

// Print schema of relName
RC Help(const char *relName) {

}

// Print relName contents
RC Print(const char *relName) {

}

// Set parameter to value
RC Set(const char *paramName, const char *value) {

}
