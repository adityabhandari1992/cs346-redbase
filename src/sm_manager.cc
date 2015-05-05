//
// File:        sm_manager.cc
// Description: SM_Manager class implementation
// Authors:     Aditya Bhandari (adityasb@stanford.edu)
//

#include <cstdio>
#include <iostream>
#include "redbase.h"
#include "sm.h"
#include "ix.h"
#include "rm.h"
using namespace std;

// Constructor
SM_Manager::SM_Manager(IX_Manager &ixm, RM_Manager &rmm) {
    // Store the RM and IX managers
    this->rmManager = &rmm;
    this->ixManager = &ixm;
}

// Destructor
SM_Manager::~SM_Manager() {
    // Nothing to free
}

// Open the database
RC SM_Manager::OpenDb(const char *dbName) {
    return (0);
}

// Close the database
RC SM_Manager::CloseDb() {
    return (0);
}

// Create relation relName, given number of attributes and attribute data
RC SM_Manager::CreateTable(const char *relName, int attrCount, AttrInfo *attributes) {
    cout << "CreateTable\n"
         << "   relName     =" << relName << "\n"
         << "   attrCount   =" << attrCount << "\n";
    for (int i = 0; i < attrCount; i++)
        cout << "   attributes[" << i << "].attrName=" << attributes[i].attrName
             << "   attrType="
             << (attributes[i].attrType == INT ? "INT" :
                 attributes[i].attrType == FLOAT ? "FLOAT" : "STRING")
             << "   attrLength=" << attributes[i].attrLength << "\n";
    return (0);
}

// Destroy a relation
RC SM_Manager::DropTable(const char *relName) {
    cout << "DropTable\n   relName=" << relName << "\n";
    return (0);
}

// Create an index for relName.attrName
RC SM_Manager::CreateIndex(const char *relName, const char *attrName) {
    cout << "CreateIndex\n"
         << "   relName =" << relName << "\n"
         << "   attrName=" << attrName << "\n";
    return (0);
}

// Destroy index on relName.attrName
RC SM_Manager::DropIndex(const char *relName, const char *attrName) {
    cout << "DropIndex\n"
         << "   relName =" << relName << "\n"
         << "   attrName=" << attrName << "\n";
    return (0);
}

// Load relName from fileName
RC SM_Manager::Load(const char *relName, const char *fileName) {
    cout << "Load\n"
         << "   relName =" << relName << "\n"
         << "   fileName=" << fileName << "\n";
    return (0);
}

// Print relations in db
RC SM_Manager::Help() {
    cout << "Help\n";
    return (0);
}

// Print schema of relName
RC SM_Manager::Help(const char *relName) {
    cout << "Help\n"
         << "   relName=" << relName << "\n";
    return (0);
}

// Print relName contents
RC SM_Manager::Print(const char *relName) {
    cout << "Print\n"
         << "   relName=" << relName << "\n";
    return (0);
}

// Set parameter to value
RC SM_Manager::Set(const char *paramName, const char *value) {
    cout << "Set\n"
         << "   paramName=" << paramName << "\n"
         << "   value    =" << value << "\n";
    return (0);
}
