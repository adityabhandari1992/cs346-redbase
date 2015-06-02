//
// File:        ex_commlayer.cc
// Description: EX_CommLayer class implementation
// Authors:     Aditya Bhandari (adityasb@stanford.edu)
//

#include <cstdio>
#include <unistd.h>
#include <iostream>
#include <string>
#include <sstream>
#include "redbase.h"
#include "rm.h"
#include "ix.h"
#include "sm.h"
#include "ql.h"
#include "ql_internal.h"
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

// Destructor
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
    if ((rc = smManager->OpenDb(dataNode.c_str()))) {
        return rc;
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
    if ((rc = smManager->CloseDb())) {
        return rc;
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


// Method: LoadInDataNode(const char* relName, vector<string> nodeTuples, int node)
// Load the tuples in the vector to the relation in the data node
RC EX_CommLayer::LoadInDataNode(const char* relName, vector<string> nodeTuples, int node) {
    int rc;
    string dataNode = "data." + to_string(node);

    // Open the data node
    if ((rc = smManager->OpenDb(dataNode.c_str()))) {
        return rc;
    }

    // Get the relation and attributes information
    SM_RelcatRecord* rcRecord = new SM_RelcatRecord;
    memset(rcRecord, 0, sizeof(SM_RelcatRecord));
    if ((rc = smManager->GetRelInfo(relName, rcRecord))) {
        delete rcRecord;
        return rc;
    }
    int tupleLength = rcRecord->tupleLength;
    int attrCount = rcRecord->attrCount;
    int indexCount = rcRecord->indexCount;

    DataAttrInfo* attributes = new DataAttrInfo[attrCount];
    if ((rc = smManager->GetAttrInfo(relName, attrCount, (char*) attributes))) {
        return rc;
    }
    char* tupleData = new char[tupleLength];
    memset(tupleData, 0, tupleLength);

    // Open the RM file
    RM_FileHandle rmFH;
    RID rid;
    if ((rc = rmManager->OpenFile(relName, rmFH))) {
        return rc;
    }

    // Open the indexes
    IX_IndexHandle* ixIH = new IX_IndexHandle[attrCount];
    if (indexCount > 0) {
        int currentIndex = 0;
        for (int i=0; i<attrCount; i++) {
            int indexNo = attributes[i].indexNo;
            if (indexNo != -1) {
                if (currentIndex == indexCount) {
                    return SM_INCORRECT_INDEX_COUNT;
                }
                if ((rc = ixManager->OpenIndex(relName, indexNo, ixIH[currentIndex]))) {
                    return rc;
                }
                currentIndex++;
            }
        }
    }

    // Read each tuple from the vector
    int numberTuples = nodeTuples.size();
    for (int k=0; k<numberTuples; k++) {
        // Parse the tuple
        stringstream ss(nodeTuples[k]);
        vector<string> dataValues;
        string dataValue = "";
        while (getline(ss, dataValue, ',')) {
            dataValues.push_back(dataValue);
        }

        // Insert the tuple in the relation
        for (int i=0; i<attrCount; i++) {
            if (attributes[i].attrType == INT) {
                int value = atoi(dataValues[i].c_str());
                memcpy(tupleData+attributes[i].offset, &value, attributes[i].attrLength);
            }
            else if (attributes[i].attrType == FLOAT) {
                float value = atof(dataValues[i].c_str());
                memcpy(tupleData+attributes[i].offset, &value, attributes[i].attrLength);
            }
            else {
                char value[attributes[i].attrLength];
                memset(value, 0, attributes[i].attrLength);
                strcpy(value, dataValues[i].c_str());
                memcpy(tupleData+attributes[i].offset, value, attributes[i].attrLength);
            }
        }
        if ((rc = rmFH.InsertRec(tupleData, rid))) {
            return rc;
        }

        // Insert the entries in the indexes
        int currentIndex = 0;
        for (int i=0; i<attrCount; i++) {
            if (attributes[i].indexNo != -1) {
                if (attributes[i].attrType == INT) {
                    int value = atoi(dataValues[i].c_str());
                    if ((rc = ixIH[currentIndex].InsertEntry(&value, rid))) {
                        return rc;
                    }
                }
                else if (attributes[i].attrType == FLOAT) {
                    float value = atof(dataValues[i].c_str());
                    if ((rc = ixIH[currentIndex].InsertEntry(&value, rid))) {
                        return rc;
                    }
                }
                else {
                    char* value = (char*) dataValues[i].c_str();
                    if ((rc = ixIH[currentIndex].InsertEntry(value, rid))) {
                        return rc;
                    }
                }
                currentIndex++;
            }
        }
    }

    // Close the RM file
    if ((rc = rmManager->CloseFile(rmFH))) {
        return rc;
    }

    // Close the indexes
    if (indexCount > 0) {
        for (int i=0; i<indexCount; i++) {
            if ((rc = ixManager->CloseIndex(ixIH[i]))) {
                return rc;
            }
        }
    }

    delete rcRecord;
    delete[] attributes;
    delete[] ixIH;
    delete[] tupleData;

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
    cout << "\n* In data node number " << node << " *" << endl;
    if ((rc = qlManager->Insert(relName, nValues, values))) {
        return rc;
    }

    // Close the data node
    if ((rc = smManager->CloseDb())) {
        return rc;
    }

    return OK_RC;
}


// Method: InsertInDataNode(const char* relName, const char* recordData, int node)
// Insert a tuple in the data node
RC EX_CommLayer::InsertInDataNode(const char* relName, const char* recordData, int node) {
    int rc;
    string dataNode = "data." + to_string(node);

    // Open the data node
    if ((rc = smManager->OpenDb(dataNode.c_str()))) {
        return rc;
    }

    // Open the RM File
    RM_FileHandle rmFH;
    if ((rc = rmManager->OpenFile(relName, rmFH))) {
        return rc;
    }

    // Insert the record
    RID rid;
    if ((rc = rmFH.InsertRec(recordData, rid))) {
        return rc;
    }

    // Close the file
    if ((rc = rmManager->CloseFile(rmFH))) {
        return rc;
    }

    // Close the data node
    if ((rc = smManager->CloseDb())) {
        return rc;
    }

    return OK_RC;
}


// Method: DeleteInDataNode(const char* relName, int nConditions, const Condition conditions[])
// Pass a delete query to the data node
RC EX_CommLayer::DeleteInDataNode(const char* relName, int nConditions, const Condition conditions[], int node) {
    int rc;
    string dataNode = "data." + to_string(node);

    // Open the data node
    if ((rc = smManager->OpenDb(dataNode.c_str()))) {
        return rc;
    }

    // Execute the delete query
    cout << "\n* In data node number " << node << " *" << endl;
    if ((rc = qlManager->Delete(relName, nConditions, conditions))) {
        return rc;
    }

    // Close the data node
    if ((rc = smManager->CloseDb())) {
        return rc;
    }

    return OK_RC;
}


// Method: UpdateInDataNode(const char* relName, const RelAttr &updAttr, const int bIsValue, const RelAttr &rhsRelAttr,
//                          const Value &rhsValue, int nConditions, const Condition conditions[], int node)
// Pass an update query to the data node
RC EX_CommLayer::UpdateInDataNode(const char* relName, const RelAttr &updAttr, const int bIsValue, const RelAttr &rhsRelAttr,
                                  const Value &rhsValue, int nConditions, const Condition conditions[], int node, bool reshuffle) {
    int rc;
    string dataNode = "data." + to_string(node);

    // Open the data node
    if ((rc = smManager->OpenDb(dataNode.c_str()))) {
        return rc;
    }

    // Execute the delete query
    cout << "\n* In data node number " << node << " *" << endl;
    if ((rc = qlManager->Update(relName, updAttr, bIsValue, rhsRelAttr, rhsValue, nConditions, conditions))) {
        return rc;
    }

    // If need to reshuffle
    if (reshuffle) {
        // Get the relation and attribute information
        SM_RelcatRecord* rcRecord = new SM_RelcatRecord;
        memset(rcRecord, 0, sizeof(SM_RelcatRecord));
        if ((rc = smManager->GetRelInfo(relName, rcRecord))) {
            delete rcRecord;
            return rc;
        }
        int attrCount = rcRecord->attrCount;

        DataAttrInfo* attributes = new DataAttrInfo[attrCount];
        if ((rc = smManager->GetAttrInfo(relName, attrCount, (char*) attributes))) {
            return rc;
        }

        int attrIndex = 0;
        for (int i=0; i<attrCount; i++) {
            if (strcmp(attributes[i].attrName, updAttr.attrName) == 0) {
                attrIndex = i;
                break;
            }
        }

        // Open file scan on relation
        RM_FileHandle rmFH;
        RM_FileScan rmFS;
        RM_Record rec;
        char* recordData;
        if ((rc = rmManager->OpenFile(relName, rmFH))) {
            return rc;
        }
        if ((rc = rmFS.OpenScan(rmFH, INT, 4, 0, NO_OP, NULL))) {
            return rc;
        }

        // Close the data node
        if ((rc = smManager->CloseDb())) {
            return rc;
        }

        // Check each record
        while ((rc = rmFS.GetNextRec(rec)) != RM_EOF) {
            if ((rc = rec.GetData(recordData))) {
                return rc;
            }

            // Form the value key
            int dataNode = 0;
            Value key;
            key.type = attributes[attrIndex].attrType;
            if (attributes[attrIndex].attrType == INT) {
                int* keyValue = new int;
                memcpy(keyValue, recordData+attributes[attrIndex].offset, sizeof(int));
                key.data = keyValue;

                // Get the node for the key
                if ((rc = GetDataNodeForTuple(rmManager, key, relName, updAttr.attrName, dataNode))) {
                    return rc;
                }
                delete keyValue;
            }
            else if (attributes[attrIndex].attrType == FLOAT) {
                float* keyValue = new float;
                memcpy(keyValue, recordData+attributes[attrIndex].offset, sizeof(float));
                key.data = keyValue;

                // Get the node for the key
                if ((rc = GetDataNodeForTuple(rmManager, key, relName, updAttr.attrName, dataNode))) {
                    return rc;
                }
                delete keyValue;
            }
            else {
                char* keyValue = new char[attributes[attrIndex].attrLength];
                strcpy(keyValue, recordData+attributes[attrIndex].offset);
                key.data = keyValue;

                // Get the node for the key
                if ((rc = GetDataNodeForTuple(rmManager, key, relName, updAttr.attrName, dataNode))) {
                    return rc;
                }
                delete[] keyValue;
            }

            // Reshuffle the record if needed
            if (dataNode != node) {
                // Get the rid and delete
                RID rid;
                if ((rc = rec.GetRid(rid))) {
                    return rc;
                }
                if ((rc = rmFH.DeleteRec(rid))) {
                    return rc;
                }

                // Insert in the correct data node
                if ((rc = InsertInDataNode(relName, recordData, dataNode))) {
                    return rc;
                }
            }
        }

        // Close the scan and file
        if ((rc = rmFS.CloseScan())) {
            return rc;
        }
        if ((rc = rmManager->CloseFile(rmFH))) {
            return rc;
        }

        delete rcRecord;
        delete[] attributes;
    }

    // No need to reshuffle
    else {
        // Close the data node
        if ((rc = smManager->CloseDb())) {
            return rc;
        }
    }

    return OK_RC;
}


// Method: GetDataFromDataNode(const char* relName, RM_FileHandle &tempRMFH, int node,
//                             bool isCond, Condition* filterCond, Condition conditions[], int &nConditions)
// Get data for a relation from the data node
RC EX_CommLayer::GetDataFromDataNode(const char* relName, RM_FileHandle &tempRMFH, int node,
                                     bool isCond, Condition* filterCond, Condition conditions[], int &nConditions) {
    int rc;
    string dataNode = "data." + to_string(node);

    // Open the data node
    if ((rc = smManager->OpenDb(dataNode.c_str()))) {
        return rc;
    }

    // Get the relation and attributes information
    SM_RelcatRecord* rcRecord = new SM_RelcatRecord;
    memset(rcRecord, 0, sizeof(SM_RelcatRecord));
    if ((rc = smManager->GetRelInfo(relName, rcRecord))) {
        delete rcRecord;
        return rc;
    }
    int attrCount = rcRecord->attrCount;
    DataAttrInfo* attributes = new DataAttrInfo[attrCount];
    if ((rc = smManager->GetAttrInfo(relName, attrCount, (char*) attributes))) {
        return rc;
    }

    // Check if index scan is possible
    bool indexScan = false;
    int indexCondition = -1;
    DataAttrInfo* attributeData = new DataAttrInfo;
    for (int i=0; i<nConditions; i++) {
        Condition cond = conditions[i];
        if (strcmp((cond.lhsAttr).relName, relName) == 0 && !cond.bRhsIsAttr) {
            if ((rc = GetAttrInfoFromArray((char*) attributes, attrCount, relName, (cond.lhsAttr).attrName, (char*) attributeData))) {
                return rc;
            }
            if (attributeData->indexNo != -1) {
                indexScan = true;
                indexCondition = i;
                break;
            }
        }
    }
    delete rcRecord;
    delete attributeData;
    delete[] attributes;

    // Create the scan operator
    shared_ptr<QL_Op> rootOp;
    shared_ptr<QL_Op> scanOp;
    if (indexScan) {
        Condition cond = conditions[indexCondition];
        scanOp.reset(new QL_IndexScanOp(smManager, ixManager, rmManager, relName, (cond.lhsAttr).attrName, cond.op, &cond.rhsValue));
        RemoveCondition(conditions, nConditions, indexCondition);
        rootOp = scanOp;

        // Create a filter operation in case of additional condition
        if (isCond) {
            rootOp.reset(new QL_FilterOp(smManager, scanOp, *filterCond));
        }
    }

    // Else do a file scan
    if (!indexScan) {
        if (isCond) {
            scanOp.reset(new QL_FileScanOp(smManager, rmManager, relName, true, (filterCond->lhsAttr).attrName, filterCond->op, &(filterCond->rhsValue)));
        }
        else {
            scanOp.reset(new QL_FileScanOp(smManager, rmManager, relName, false, NULL, NO_OP, NULL));
        }
        rootOp = scanOp;
    }

    // Create and open the shuffle operator
    QL_ShuffleDataOp* shuffleOp = new QL_ShuffleDataOp(rmManager, rootOp, node, 0);
    if ((rc = shuffleOp->Open())) {
        return rc;
    }

    // Get the data via the shuffle operator
    if ((rc = shuffleOp->GetData(tempRMFH))) {
        return rc;
    }

    // Close the operator
    if ((rc = shuffleOp->Close())) {
        return rc;
    }

    // Print the shuffle operator
    if (bQueryPlans) {
        shuffleOp->Print(0);
    }
    delete shuffleOp;

    // Close the data node
    if ((rc = smManager->CloseDb())) {
        return rc;
    }

    return OK_RC;
}


/***** Helper methods for EX part *****/

// Method: GetDataNodeForTuple(RM_Manager* rmManager, const Value key, const char* relName,
//                             const char* attrName, int &node)
// Get the data node number for the required tuple based on the partition vector
RC GetDataNodeForTuple(RM_Manager* rmManager, const Value key, const char* relName,
                       const char* attrName, int &node) {
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
            char* givenValueChar = static_cast<char*>(key.data);
            EX_StringPartitionVectorRecord* pV = (EX_StringPartitionVectorRecord*) recordData;
            string givenValue(givenValueChar);
            string startValue(pV->startValue);
            string endValue(pV->endValue);
            if (givenValue >= startValue && givenValue < endValue) {
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


// Method: CheckDataNodeForCondition(RM_Manager* rmManager, const char* relName, const char* attrName,
//                                   Condition checkCondition, int node, bool &valid)
// Check the data node whether its partition range satisfies the condition
RC CheckDataNodeForCondition(RM_Manager* rmManager, const char* relName, const char* attrName,
                             Condition checkCondition, int node, bool &valid) {
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

    // Scan the file for the required node
    RM_FileScan rmFS;
    RM_Record rec;
    char* recordData;
    if ((rc = rmFS.OpenScan(rmFH, INT, 4, 0, EQ_OP, &node))) {
        return rc;
    }

    // Get the record
    if ((rc = rmFS.GetNextRec(rec)) == RM_EOF) {
        return EX_INCONSISTENT_PV;
    }
    else {
        // Get the record data
        if ((rc = rec.GetData(recordData))) {
            return rc;
        }

        // Check the start and end values
        AttrType partitionAttrType = checkCondition.rhsValue.type;
        if (partitionAttrType == INT) {
            EX_IntPartitionVectorRecord* pv = (EX_IntPartitionVectorRecord*) recordData;
            int value = *static_cast<int*>(checkCondition.rhsValue.data);
            valid = MatchValues(pv->startValue, pv->endValue, checkCondition.op, value);
        }
        else if (partitionAttrType == FLOAT) {
            EX_FloatPartitionVectorRecord* pv = (EX_FloatPartitionVectorRecord*) recordData;
            float value = *static_cast<float*>(checkCondition.rhsValue.data);
            valid = MatchValues(pv->startValue, pv->endValue, checkCondition.op, value);
        }
        else {
            EX_StringPartitionVectorRecord* pv = (EX_StringPartitionVectorRecord*) recordData;
            char* valueChar = static_cast<char*>(checkCondition.rhsValue.data);
            string value(valueChar);
            string startValue(pv->startValue);
            string endValue(pv->endValue);
            valid = MatchValues(startValue, endValue, checkCondition.op, value);
        }
    }

    // Close the scan and file
    if ((rc = rmFS.CloseScan())){
        return rc;
    }
    if ((rc = rmManager->CloseFile(rmFH))) {
        return rc;
    }

    return OK_RC;
}


// Method: bool MatchValues(T start, T end, CompOp op, T value)
// Check whether a record from the partition vector satisfies the condition (op value)
template <typename T>
bool MatchValues(T start, T end, CompOp op, T value) {
    switch(op) {
        case EQ_OP:
            return (value >= start && value < end);
        case LT_OP:
            return (start < value);
        case GT_OP:
            return (end > value);
        case LE_OP:
            return (start <= value);
        case GE_OP:
            return (end > value);
        default:
            return true;
    }
}
