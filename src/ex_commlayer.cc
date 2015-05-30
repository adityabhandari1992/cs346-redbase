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


/***** EX_CommLayer class */

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

// Method: InsertInDataNode(const char* relName, int nValues, const Value values[], int node)
// Insert a tuple in the data node
RC EX_CommLayer::InsertInDataNode(const char* relName, int nValues, const Value values[], int node) {
    int rc;
    string dataNode = "data." + to_string(node);

    // Open the data node
    if ((rc = smManager->OpenDb(dataNode.c_str()))) {
        return rc;
    }

    // Insert the tuple
    if ((rc = qlManager->Insert(relName, nValues, values))) {
        return rc;
    }

    // Close the data node
    if ((rc = smManager->CloseDb())) {
        return rc;
    }

    return OK_RC;
}


/***** Helper methods for EX part *****/

// Method: GetDataNodeForTuple(RM_Manager* rmManager, const Value key, const char* relName, const char* attrName, int &node)
// Get the data node number for the required tuple based on the partition vector
RC GetDataNodeForTuple(RM_Manager* rmManager, const Value key, const char* relName, const char* attrName, int &node) {
    // Get the type
    AttrType attrType = key.type;

    // Open the RM file
    char partitionVectorFileName[255];
    strcpy(partitionVectorFileName, relName);
    strcat(partitionVectorFileName, "_partitions_");
    strcat(partitionVectorFileName, attrName);

    int rc;
    RM_FileHandle rmFH;
    if ((rc = rmManager->OpenFile(partitionVectorFileName, rmFH))) {
        return rc;
    }

    // Scan the file for the required value
    RM_FileScan rmFS;
    RM_Record rec;
    char* recordData;
    if ((rc = rmFS.OpenScan(rmFH, INT, 4, 0, NO_OP, NULL))) {
        return rc;
    }
    while ((rc = rmFS.GetNextRec(rec)) != RM_EOF) {
        // Get the record data
        if ((rc = rec.GetData(recordData))) {
            return rc;
        }

        // Check for each type
        if (attrType == INT) {
            int givenValue = *static_cast<int*>(key.data);
            EX_IntPartitionVectorRecord* pV = (EX_IntPartitionVectorRecord*) recordData;
            if (givenValue >= pV->startValue && givenValue < pV->endValue) {
                node = pV->node;
                break;
            }
        }

        else if (attrType == FLOAT) {
            float givenValue = *static_cast<float*>(key.data);
            EX_FloatPartitionVectorRecord* pV = (EX_FloatPartitionVectorRecord*) recordData;
            if (givenValue >= pV->startValue && givenValue < pV->endValue) {
                node = pV->node;
                break;
            }
        }

        else {
            char* givenValue = static_cast<char*>(key.data);
            EX_StringPartitionVectorRecord* pV = (EX_StringPartitionVectorRecord*) recordData;
            if (strcmp(givenValue, pV->startValue) >= 0 && strcmp(givenValue, pV->endValue) < 0) {
                node = pV->node;
                break;
            }
        }
    }

    // Check if the value was found
    if (rc == RM_EOF) {
        return EX_INCONSISTENT_PV;
    }

    // Close the scan and file
    if ((rc = rmFS.CloseScan())) {
        return rc;
    }
    if ((rc = rmManager->CloseFile(rmFH))) {
        return rc;
    }

    return OK_RC;
}
