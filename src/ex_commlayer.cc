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

// Method: PrintInDataNode(const char* relName, int node)
// Print the tuples for a relation in the data node
RC EX_CommLayer::PrintInDataNode(Printer &p, const char* relName, int node) {
    int rc;
    RM_Record rec;
    char* recordData;
    string dataNode = "data." + to_string(node);

    // Open the data node
    if (chdir(dataNode.c_str()) < 0) {
        return EX_INVALID_DATA_NODE;
    }

    // Open the relation RM file
    RM_FileHandle rmFH;
    if ((rc = rmManager->OpenFile(relName, rmFH))) {
        return SM_TABLE_DOES_NOT_EXIST;
    }

    // Start the relcat file scan
    RM_FileScan rmFS;
    if ((rc = rmFS.OpenScan(rmFH, INT, 4, 0, NO_OP, NULL))) {
        return rc;
    }

    // Print each tuple
    while (rc != RM_EOF) {
        rc = rmFS.GetNextRec(rec);

        if (rc != 0 && rc != RM_EOF) {
            return rc;
        }

        if (rc != RM_EOF) {
            rec.GetData(recordData);
            p.Print(cout, recordData);
        }
    }

    // Close the scan and file and clean up
    if ((rc = rmFS.CloseScan())) {
        return rc;
    }
    if ((rc = rmManager->CloseFile(rmFH))) {
        return rc;
    }

    // Close the data node
    if (chdir("../") < 0) {
        return EX_INVALID_DATA_NODE;
    }

    return OK_RC;
}

// Method: CreateIndexInDataNode(const char* relName, const char* attrName, int node)
// Create an index in the data node
RC EX_CommLayer::CreateIndexInDataNode(const char* relName, const char* attrName, int node) {
    int rc;
    string dataNode = "data." + to_string(node);

    // Open the data node
    if ((rc = smManager->OpenDb(dataNode.c_str()))) {
        return rc;
    }

    // Create the index
    if ((rc = smManager->CreateIndex(relName, attrName))) {
        return rc;
    }

    // Close the data node
    if ((rc = smManager->CloseDb())) {
        return rc;
    }

    return OK_RC;
}

// Method: DropIndexInDataNode(const char* relName, const char* attrName, int node)
// Create an index in the data node
RC EX_CommLayer::DropIndexInDataNode(const char* relName, const char* attrName, int node) {
    int rc;
    string dataNode = "data." + to_string(node);

    // Open the data node
    if ((rc = smManager->OpenDb(dataNode.c_str()))) {
        return rc;
    }

    // Drop the index
    if ((rc = smManager->DropIndex(relName, attrName))) {
        return rc;
    }

    // Close the data node
    if ((rc = smManager->CloseDb())) {
        return rc;
    }

    return OK_RC;
}
