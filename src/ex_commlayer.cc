//
// File:        ex_commlayer.cc
// Description: EX_CommLayer class implementation
// Authors:     Aditya Bhandari (adityasb@stanford.edu)
//

#include <cstdio>
#include <unistd.h>
#include <iostream>
#include <string>
#include "redbase.h"
#include "rm.h"
#include "ix.h"
#include "sm.h"
#include "ql.h"
#include "ex.h"
#include "printer.h"
#include "parser.h"
using namespace std;

// Constructor
EX_CommLayer::EX_CommLayer(RM_Manager* rmm, IX_Manager* ixm) {
    // Set the class members
    rmManager = rmm;
    ixManager = ixm;

    // Create SM and QL managers
    smManager = new SM_Manager(*ixManager, *rmManager);
    qlManager = new QL_Manager(*smManager, *ixManager, *rmManager);
}

EX_CommLayer::~EX_CommLayer() {
    // Delete SM and QL managers
    delete smManager;
    delete qlManager;
}

// Method: CreateTableInDataNode(const char* relName, int attrCount, AttrInfo* attributes, int node)
// Create a table in the data node
RC EX_CommLayer::CreateTableInDataNode(const char* relName, int attrCount, AttrInfo* attributes, int node) {
    int rc;
    string dataNode = "data." + to_string(node);

    // Open the database
    if ((rc = smManager->OpenDb(dataNode.c_str()))) {
        return rc;
    }

    // Create the table
    if ((rc = smManager->CreateTable(relName, attrCount, attributes, FALSE, NULL, 0, NULL))) {
        return rc;
    }

    // Close the database
    if ((rc = smManager->CloseDb())) {
        return rc;
    }

    return OK_RC;
}

// Method: DropTableInDataNode(const char* relName, int node)
// Drop a table from the data node
RC EX_CommLayer::DropTableInDataNode(const char* relName, int node) {
    int rc;
    string dataNode = "data." + to_string(node);

    // Open the data node
    if ((rc = smManager->OpenDb(dataNode.c_str()))) {
        return rc;
    }

    // Drop the table
    if ((rc = smManager->DropTable(relName))) {
        return rc;
    }

    // Close the data node
    if ((rc = smManager->CloseDb())) {
        return rc;
    }

    return OK_RC;
}
