//
// File:        ql_manager.cc
// Description: QL_Manager class implementation
// Authors:     Aditya Bhandari (adityasb@stanford.edu)
//

#include <cstdio>
#include <iostream>
#include <sys/times.h>
#include <sys/types.h>
#include <cassert>
#include <unistd.h>
#include <sstream>
#include <memory>
#include "redbase.h"
#include "ql.h"
#include "ql_internal.h"
#include "ex.h"
#include "sm.h"
#include "ix.h"
#include "rm.h"
#include "printer.h"
#include "parser.h"

using namespace std;

// Method: QL_Manager(SM_Manager &smm, IX_Manager &ixm, RM_Manager &rmm)
// Constructor for the QL Manager
QL_Manager::QL_Manager(SM_Manager &smm, IX_Manager &ixm, RM_Manager &rmm) {
    // Store the objects
    this->rmManager = &rmm;
    this->ixManager = &ixm;
    this->smManager = &smm;
}

// Method: ~QL_Manager()
// Destructor for the QL Manager
QL_Manager::~QL_Manager() {
    // Nothing to free
}


/************ SELECT ************/

// Method:
// Handle the select clause
/* Steps:
    1) Check whether the database is open
    2) Obtain attribute information for the relations and check
    3) Validate the selection expressions
    4) Validate the conditions
    5) Form the physical operator tree
    6) Get the tuples from the root node
    7) Print the physical query plan

*/
RC QL_Manager::Select(int nSelAttrs, const RelAttr selAttrs[],
                      int nRelations, const char * const relations[],
                      int nConditions, const Condition conditions[]) {
    // Check whether database is open
    if (!smManager->getOpenFlag()) {
        return QL_DATABASE_CLOSED;
    }

    // Check if the relations exist
    int rc;
    SM_RelcatRecord* rcRecords[nRelations];
    DataAttrInfo* attributes[nRelations];
    int attrCount[nRelations];
    for (int i=0; i<nRelations; i++) {
        rcRecords[i] = new SM_RelcatRecord;
        memset(rcRecords[i], 0, sizeof(SM_RelcatRecord));
        if ((rc = smManager->GetRelInfo(relations[i], rcRecords[i]))) {
            for (int j=0; j<=i; j++) delete rcRecords[j];
            for (int j=0; j<i; j++) delete[] attributes[j];
            return rc;
        }
        attrCount[i] = rcRecords[i]->attrCount;
        attributes[i] = new DataAttrInfo[attrCount[i]];
        if ((rc = smManager->GetAttrInfo(relations[i], attrCount[i], (char*) attributes[i]))) {
            return rc;
        }
    }

    // Check for duplicate relations
    for (int i=0; i<nRelations; i++) {
        for (int j=0; j<nRelations; j++) {
            if (j != i && strcmp(rcRecords[i]->relName, rcRecords[j]->relName) == 0) {
                for (int k=0; k<nRelations; k++) {
                    delete rcRecords[k];
                    delete[] attributes[k];
                }
                return QL_INVALID_FROM_CLAUSE;
            }
        }
    }

    // Validate the select attributes
    RelAttr* changedSelAttrs;
    if (nSelAttrs == 1 && strcmp(selAttrs[0].attrName, "*") == 0) {
        // In case of *, select all attributes
        nSelAttrs = 0;
        for (int i=0; i<nRelations; i++) {
            nSelAttrs += rcRecords[i]->attrCount;
        }
        changedSelAttrs = new RelAttr[nSelAttrs];
        int index = 0;
        for (int i=0; i<nRelations; i++) {
            for (int j=0; j<rcRecords[i]->attrCount; j++) {
                changedSelAttrs[index].relName = rcRecords[i]->relName;
                changedSelAttrs[index].attrName = attributes[i][j].attrName;
                index++;
            }
        }
    }
    else {
        changedSelAttrs = (RelAttr*) selAttrs;
        SM_AttrcatRecord* attributeData = new SM_AttrcatRecord;
        for (int i=0; i<nSelAttrs; i++) {
            // If relName is not specified, check if unique
            if (changedSelAttrs[i].relName == NULL) {
                // Check all the relations in the relations array
                int found = 1;
                for (int j=0; j<nRelations; j++) {
                    rc = smManager->GetAttrInfo(relations[j], changedSelAttrs[i].attrName, attributeData);
                    if (found == 0 && rc == 0) {
                        delete attributeData;
                        for (int k=0; k<nRelations; k++) {
                            delete rcRecords[k];
                            delete[] attributes[k];
                        }
                        return QL_INVALID_ATTRIBUTE;
                    }
                    else if (rc == 0) {
                        changedSelAttrs[i].relName = (char*) relations[j];
                    }
                    found = found && rc;
                }
                if (found) {
                    delete attributeData;
                    for (int j=0; j<nRelations; j++) {
                        delete rcRecords[j];
                        delete[] attributes[j];
                    }
                    return QL_INVALID_ATTRIBUTE;
                }
            }

            // Else check if the attribute exists in the given relName
            else {
                if ((rc = smManager->GetAttrInfo(selAttrs[i].relName, selAttrs[i].attrName, attributeData))) {
                    delete attributeData;
                    for (int j=0; j<nRelations; j++) {
                        delete rcRecords[j];
                        delete[] attributes[j];
                    }
                    return rc;
                }
            }
        }
        delete attributeData;
    }

    // Validate the conditions
    Condition changedConditions[nConditions];
    for (int i=0; i<nConditions; i++) {
        changedConditions[i] = conditions[i];
    }
    if ((rc = ValidateConditionsMultipleRelations(rcRecords, (char**) attributes, nRelations, nConditions, changedConditions))) {
        for (int i=0; i<nRelations; i++) {
            delete rcRecords[i];
            delete[] attributes[i];
        }
        return rc;
    }

    // EX - Get the data for the distributed relations
    EX_CommLayer commLayer(rmManager, ixManager);
    int numberNodes = smManager->getNumberNodes();
    for (int i=0; i<nRelations; i++) {
        if (rcRecords[i]->distributed) {
            char partitionAttrName[MAXNAME+1];
            strcpy(partitionAttrName, rcRecords[i]->attrName);

            // Print message
            if (bQueryPlans) {
                cout << "\n* Getting data for " << relations[i] << " *" << endl;
            }

            // Get the data for the distributed relation in a temporary file
            RM_FileHandle tempRMFH;
            if ((rc = rmManager->CreateFile(relations[i], rcRecords[i]->tupleLength))) {
                return rc;
            }
            if ((rc = rmManager->OpenFile(relations[i], tempRMFH))) {
                return rc;
            }

            // Check whether the partition attribute is used in a condition
            bool condExists = false;
            int conditionNumber = -1;
            for (int j=0; j<nConditions; j++) {
                Condition currentCondition = changedConditions[j];
                char* lhsRelName = (currentCondition.lhsAttr).relName;
                char* lhsAttrName = (currentCondition.lhsAttr).attrName;
                int rhsIsAttr = currentCondition.bRhsIsAttr;
                if (!rhsIsAttr) {
                    if (strcmp(lhsRelName, relations[i]) == 0 && strcmp(lhsAttrName, partitionAttrName) == 0) {
                        condExists = true;
                        conditionNumber = j;
                        break;
                    }
                }
            }

            // If condition exists, get the data node only from the required data nodes
            if (condExists) {
                for (int j=1; j<=numberNodes; j++) {
                    bool valid = false;
                    if ((rc = CheckDataNodeForCondition(rmManager, relations[i], partitionAttrName, changedConditions[conditionNumber], j, valid))) {
                        return rc;
                    }
                    if (valid) {
                        if ((rc = commLayer.GetDataFromDataNode(relations[i], tempRMFH, j, true, &changedConditions[conditionNumber], changedConditions, nConditions))) {
                            return rc;
                        }
                    }
                }

                // Remove the used condition
                RemoveCondition(changedConditions, nConditions, conditionNumber);
            }

            // Else get data from all nodes
            else {
                for (int j=1; j<=numberNodes; j++) {
                    if ((rc = commLayer.GetDataFromDataNode(relations[i], tempRMFH, j, false, NULL, changedConditions, nConditions))) {
                        return rc;
                    }
                }
            }

            // Close the temporary file
            if ((rc = rmManager->CloseFile(tempRMFH))) {
                return rc;
            }
        }
    }

    // Form the physical operator tree (physical query plan)

    /** No optimizations
        1) FileScanOps as leaf nodes
        2) CrossProductOps on the leaf nodes
        3) FilterOps on top
        4) ProjectOp as the root
    **/
    shared_ptr<QL_Op> lastOp;
    if (!smManager->getOptimizeFlag()) {
        // Scan ops - FileScans on the relations
        shared_ptr<QL_Op> scanOps[nRelations];
        for (int i=0; i<nRelations; i++) {
            scanOps[i].reset(new QL_FileScanOp(smManager, rmManager, relations[i], false, NULL, NO_OP, NULL));
        }
        lastOp = scanOps[0];

        // CrossProductOps on the scan ops
        if (nRelations > 1) {
            shared_ptr<QL_Op> cpOps[nRelations-1];
            for (int i=0; i<nRelations-1; i++) {
                cpOps[i].reset(new QL_CrossProductOp(smManager, lastOp, scanOps[i+1]));
                lastOp = cpOps[i];
            }
        }
    }

    /** Optimizations when possible -
        1) IndexScanOp at leaf nodes in place of FileScanOp
        2) NLJoinOp in place of CrossProductOp
    **/
    else {
        // Scan ops - FileScans or IndexScans on the relations
        shared_ptr<QL_Op> scanOps[nRelations];
        for (int i=0; i<nRelations; i++) {
            // Check the conditions if an index scan can be done
            bool indexScan = false;
            DataAttrInfo* attributeData = new DataAttrInfo;
            for (int j=0; j<nConditions; j++) {
                Condition cond = changedConditions[j];
                if (strcmp((cond.lhsAttr).relName, relations[i]) == 0 && !cond.bRhsIsAttr) {
                    if ((rc = GetAttrInfoFromArray((char*) attributes[i], rcRecords[i]->attrCount, rcRecords[i]->relName, (cond.lhsAttr).attrName, (char*) attributeData))) {
                        return rc;
                    }
                    if (attributeData->indexNo != -1) {
                        scanOps[i].reset(new QL_IndexScanOp(smManager, ixManager, rmManager, relations[i], (cond.lhsAttr).attrName, cond.op, &cond.rhsValue));
                        RemoveCondition(changedConditions, nConditions, j);
                        indexScan = true;
                        break;
                    }
                }
            }
            if (!indexScan) {
                scanOps[i].reset(new QL_FileScanOp(smManager, rmManager, relations[i], false, NULL, NO_OP, NULL));
            }
            delete attributeData;
        }
        lastOp = scanOps[0];

        // Join ops - CrossProductOps or NLJoinOps on the scan ops
        if (nRelations > 1) {
            shared_ptr<QL_Op> joinOps[nRelations-1];
            for (int i=0; i<nRelations-1; i++) {
                // Get the attributes of the last op
                int lastOpAttrCount;
                lastOp->GetAttributeCount(lastOpAttrCount);
                DataAttrInfo* lastOpAttributes = new DataAttrInfo[lastOpAttrCount];
                lastOp->GetAttributeInfo(lastOpAttributes);
                DataAttrInfo* attributeData = new DataAttrInfo;

                // Check whether condition exists to perform a join
                bool conditionFound = false;
                for (int j=0; j<nConditions; j++) {
                    Condition cond = changedConditions[j];
                    if (cond.bRhsIsAttr) {
                        if (strcmp((cond.lhsAttr).relName, relations[i+1]) == 0) {
                            if (!(rc = GetAttrInfoFromArray((char*) lastOpAttributes, lastOpAttrCount, (cond.rhsAttr).relName, (cond.rhsAttr).attrName, (char*) attributeData))) {
                                joinOps[i].reset(new QL_NLJoinOp(smManager, scanOps[i+1], lastOp, cond));
                                RemoveCondition(changedConditions, nConditions, j);
                                conditionFound = true;
                                break;
                            }
                        }
                        else if (strcmp((cond.rhsAttr).relName, relations[i+1]) == 0) {
                            if (!(rc = GetAttrInfoFromArray((char*) lastOpAttributes, lastOpAttrCount, (cond.lhsAttr).relName, (cond.lhsAttr).attrName, (char*) attributeData))) {
                                joinOps[i].reset(new QL_NLJoinOp(smManager, lastOp, scanOps[i+1], cond));
                                RemoveCondition(changedConditions, nConditions, j);
                                conditionFound = true;
                                break;
                            }
                        }
                    }
                }

                if (!conditionFound) {
                    joinOps[i].reset(new QL_CrossProductOp(smManager, lastOp, scanOps[i+1]));
                }

                lastOp = joinOps[i];

                delete[] lastOpAttributes;
                delete attributeData;
            }
        }
    }

    // FilterOps
    if (nConditions > 0) {
        shared_ptr<QL_Op> filterOps[nConditions];
        for (int i=0; i<nConditions; i++) {
            filterOps[i].reset(new QL_FilterOp(smManager, lastOp, changedConditions[i]));
            lastOp = filterOps[i];
        }
    }

    // Root node - ProjectOp
    shared_ptr<QL_Op> rootOp;
    rootOp.reset(new QL_ProjectOp(smManager, lastOp, nSelAttrs, changedSelAttrs));

    int finalAttrCount;
    rootOp->GetAttributeCount(finalAttrCount);
    DataAttrInfo* finalAttributes = new DataAttrInfo[finalAttrCount];
    rootOp->GetAttributeInfo(finalAttributes);
    int tupleLength = 0;
    for (int i=0; i<finalAttrCount; i++) {
        tupleLength += finalAttributes[i].attrLength;
    }

    Printer p(finalAttributes, finalAttrCount);
    p.PrintHeader(cout);

    // Get the tuples from the root node
    char* recordData = new char[tupleLength];
    rootOp->Open();
    while((rc = rootOp->GetNext(recordData)) != QL_EOF) {
        p.Print(cout, recordData);
    }
    rootOp->Close();

    p.PrintFooter(cout);

    // Print the physical query plan
    if (bQueryPlans) {
        cout << "\nPhysical Query Plan:" << endl;
        rootOp->Print(0);
    }

    // Print the command
    if (smManager->getPrintFlag()) {
        int i;
        cout << "Select\n";
        cout << "   nSelAttrs = " << nSelAttrs << "\n";
        for (i = 0; i < nSelAttrs; i++)
            cout << "   selAttrs[" << i << "]:" << selAttrs[i] << "\n";
        cout << "   nRelations = " << nRelations << "\n";
        for (i = 0; i < nRelations; i++)
            cout << "   relations[" << i << "] " << relations[i] << "\n";
        cout << "   nCondtions = " << nConditions << "\n";
        for (i = 0; i < nConditions; i++)
            cout << "   conditions[" << i << "]:" << conditions[i] << "\n";
    }

    // EX - Destroy the temporary files
    for (int i=0; i<nRelations; i++) {
        if (rcRecords[i]->distributed) {
            if ((rc = rmManager->DestroyFile(relations[i]))) {
                return rc;
            }
        }
    }

    // Clean up
    delete[] recordData;
    delete[] finalAttributes;

    for (int i=0; i<nRelations; i++) {
        delete rcRecords[i];
        delete[] attributes[i];
    }
    if (strcmp(selAttrs[0].attrName, "*") == 0) {
        delete[] changedSelAttrs;
    }

    // Return OK
    return OK_RC;
}


/************ INSERT ************/

// Method: Insert(const char *relName, int nValues, const Value values[])
// Insert the values into relName
/* Steps:
    1) Check the parameters
    2) Check whether the database is open
    3) Obtain attribute information for the relation and check
    4) Open the RM file and each index file
    5) Insert the tuple in the relation
    6) Insert the entry in the indexes
    7) Print the inserted tuple
    8) Close the files
*/
RC QL_Manager::Insert(const char *relName,
                      int nValues, const Value values[]) {
    // Check the parameters
    if (relName == NULL) {
        return QL_NULL_RELATION;
    }

    // Check whether database is open
    if (!smManager->getOpenFlag()) {
        return QL_DATABASE_CLOSED;
    }

    if (strcmp(relName, "relcat") == 0 || strcmp(relName, "attrcat") == 0) {
        return QL_SYSTEM_CATALOG;
    }

    // Print the command
    if (smManager->getPrintFlag()) {
        int i;
        cout << "Insert\n";
        cout << "   relName = " << relName << "\n";
        cout << "   nValues = " << nValues << "\n";
        for (i = 0; i < nValues; i++)
            cout << "   values[" << i << "]:" << values[i] << "\n";
    }

    // Get the relation and attributes information
    int rc;
    SM_RelcatRecord* rcRecord = new SM_RelcatRecord;
    memset(rcRecord, 0, sizeof(SM_RelcatRecord));
    if ((rc = smManager->GetRelInfo(relName, rcRecord))) {
        delete rcRecord;
        return rc;
    }
    int tupleLength = rcRecord->tupleLength;
    int attrCount = rcRecord->attrCount;
    int indexCount = rcRecord->indexCount;
    int distributedRelation = rcRecord->distributed;
    char partitionAttrName[MAXNAME+1];
    strcpy(partitionAttrName, rcRecord->attrName);

    DataAttrInfo* attributes = new DataAttrInfo[attrCount];
    if ((rc = smManager->GetAttrInfo(relName, attrCount, (char*) attributes))) {
        return rc;
    }
    char* tupleData = new char[tupleLength];
    memset(tupleData, 0, tupleLength);

    // Check the values passed
    if (nValues != attrCount) {
        return QL_INCORRECT_ATTR_COUNT;
    }
    for (int i=0; i<nValues; i++) {
        Value currentValue = values[i];
        if (currentValue.type != attributes[i].attrType) {
            return QL_INCORRECT_ATTRIBUTE_TYPE;
        }
    }

    // EX - Distributed relation case
    if (distributedRelation) {
        // Find the key for the partition vector
        Value key;
        bool found = false;
        for (int i=0; i<nValues; i++) {
            if (strcmp(attributes[i].attrName, partitionAttrName) == 0) {
                found = true;
                key = values[i];
                break;
            }
        }
        if (!found) {
            return QL_ATTRIBUTE_NOT_FOUND;
        }

        // Find the correct node to insert
        int numberNodes = smManager->getNumberNodes();
        int dataNode = 0;
        if ((rc = GetDataNodeForTuple(rmManager, key, relName, partitionAttrName, dataNode))) {
            return rc;
        }
        if (dataNode <= 0 || dataNode > numberNodes) {
            return EX_INCONSISTENT_PV;
        }

        // Insert into the corresponding data node
        EX_CommLayer commLayer(rmManager, ixManager);
        if ((rc = commLayer.InsertInDataNode(relName, nValues, values, dataNode))) {
            return rc;
        }
    }

    // Non-distributed relation case
    else {
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
                        return QL_INCORRECT_INDEX_COUNT;
                    }
                    if ((rc = ixManager->OpenIndex(relName, indexNo, ixIH[currentIndex]))) {
                        return rc;
                    }
                    currentIndex++;
                }
            }
        }

        // Insert the tuple in the relation
        for (int i=0; i<attrCount; i++) {
            Value currentValue = values[i];
            if (attributes[i].attrType == INT) {
                int value = *static_cast<int*>(currentValue.data);
                memcpy(tupleData+attributes[i].offset, &value, attributes[i].attrLength);
            }
            else if (attributes[i].attrType == FLOAT) {
                float value = *static_cast<float*>(currentValue.data);
                memcpy(tupleData+attributes[i].offset, &value, attributes[i].attrLength);
            }
            else {
                char value[attributes[i].attrLength];
                memset(value, 0, attributes[i].attrLength);
                char* valueChar = static_cast<char*>(currentValue.data);
                strcpy(value, valueChar);
                memcpy(tupleData+attributes[i].offset, valueChar, attributes[i].attrLength);
            }
        }
        if ((rc = rmFH.InsertRec(tupleData, rid))) {
            return rc;
        }

        // Insert the entries in the indexes
        int currentIndex = 0;
        for (int i=0; i<attrCount; i++) {
            Value currentValue = values[i];
            if (attributes[i].indexNo != -1) {
                if (attributes[i].attrType == INT) {
                    int value = *static_cast<int*>(currentValue.data);
                    if ((rc = ixIH[currentIndex].InsertEntry(&value, rid))) {
                        return rc;
                    }
                }
                else if (attributes[i].attrType == FLOAT) {
                    float value = *static_cast<float*>(currentValue.data);
                    if ((rc = ixIH[currentIndex].InsertEntry(&value, rid))) {
                        return rc;
                    }
                }
                else {
                    char* value = static_cast<char*>(currentValue.data);
                    if ((rc = ixIH[currentIndex].InsertEntry(value, rid))) {
                        return rc;
                    }
                }
                currentIndex++;
            }
        }

        // Print the inserted tuple
        cout << "Inserted tuple:" << endl;
        Printer p(attributes, attrCount);
        p.PrintHeader(cout);
        p.Print(cout, tupleData);
        p.PrintFooter(cout);

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
        delete[] ixIH;
    }

    // Clean up
    delete rcRecord;
    delete[] attributes;
    delete[] tupleData;

    // Return OK
    return OK_RC;
}


/************ DELETE ************/

// Method: Delete (const char *relName, int nConditions, const Condition conditions[])
// Delete from the relName all tuples that satisfy conditions
/* Steps:
    1) Check the parameters
    2) Check whether the database is open
    3) Obtain attribute information for the relation and check
    4) Check the conditions
    5) Find index on some condition
    6) If index exists
        - Open index scan
        - Find tuples and delete
        - Close index scan
    7) If no index
        - Open file scan
        - Find tuples and delete
        - Close file scan
    8) Print the deleted tuples
*/
RC QL_Manager::Delete(const char *relName,
                      int nConditions, const Condition conditions[]) {
    // Check the parameters
    if (relName == NULL) {
        return QL_NULL_RELATION;
    }

    // Check whether database is open
    if (!smManager->getOpenFlag()) {
        return QL_DATABASE_CLOSED;
    }

    if (strcmp(relName, "relcat") == 0 || strcmp(relName, "attrcat") == 0) {
        return QL_SYSTEM_CATALOG;
    }

    // Get the relation and attributes information
    int rc;
    SM_RelcatRecord* rcRecord = new SM_RelcatRecord;
    memset(rcRecord, 0, sizeof(SM_RelcatRecord));
    if ((rc = smManager->GetRelInfo(relName, rcRecord))) {
        delete rcRecord;
        return rc;
    }
    int attrCount = rcRecord->attrCount;
    int distributedRelation = rcRecord->distributed;
    char partitionAttrName[MAXNAME+1];
    strcpy(partitionAttrName, rcRecord->attrName);

    DataAttrInfo* attributes = new DataAttrInfo[attrCount];
    if ((rc = smManager->GetAttrInfo(relName, attrCount, (char*) attributes))) {
        return rc;
    }

    // Validate the conditions
    if ((rc = ValidateConditionsSingleRelation(relName, attrCount, (char*) attributes, nConditions, conditions))) {
        delete rcRecord;
        delete[] attributes;
        return rc;
    }

    // Print the command
    if (smManager->getPrintFlag()) {
        int i;
        cout << "Delete\n";
        cout << "   relName = " << relName << "\n";
        cout << "   nCondtions = " << nConditions << "\n";
        for (i = 0; i < nConditions; i++)
            cout << "   conditions[" << i << "]:" << conditions[i] << "\n";
    }

    // EX - Distributed relation case
    if (distributedRelation) {
        // Check whether the partition attribute is used in a condition
        bool condExists = false;
        int conditionNumber = -1;
        for (int i=0; i<nConditions; i++) {
            Condition currentCondition = conditions[i];
            char* lhs = (currentCondition.lhsAttr).attrName;
            int rhsIsAttr = currentCondition.bRhsIsAttr;
            if (!rhsIsAttr) {
                if (strcmp(lhs, partitionAttrName) == 0) {
                    condExists = true;
                    conditionNumber = i;
                    break;
                }
            }
        }

        // If condition on partition attribute exists
        int numberNodes = smManager->getNumberNodes();
        EX_CommLayer commLayer(rmManager, ixManager);
        if (condExists) {
            // Find the nodes to pass the query
            for (int i=1; i<=numberNodes; i++) {
                bool valid = false;
                if ((rc = CheckDataNodeForCondition(rmManager, relName, partitionAttrName, conditions[conditionNumber], i, valid))) {
                    return rc;
                }
                if (valid) {
                    // Pass the query to the node
                    if ((rc = commLayer.DeleteInDataNode(relName, nConditions, conditions, i))) {
                        return rc;
                    }
                }
            }
        }

        // Else if no such condition
        else {
            // Pass the query to all data nodes
            for (int i=1; i<=numberNodes; i++) {
                if ((rc = commLayer.DeleteInDataNode(relName, nConditions, conditions, i))) {
                    return rc;
                }
            }
        }
    }

    // Non-distributed relation case
    else {
        // Prepare the printer class
        cout << "Deleted tuples:" << endl;
        Printer p(attributes, attrCount);
        p.PrintHeader(cout);

        // Find whether index exists on some condition
        bool indexExists = false;
        int indexCondition = -1;
        for (int i=0; i<nConditions; i++) {
            if (indexExists) break;
            Condition currentCondition = conditions[i];
            char* lhs = (currentCondition.lhsAttr).attrName;
            int rhsIsAttr = currentCondition.bRhsIsAttr;
            if (!rhsIsAttr) {
                for (int j=0; j<attrCount; j++) {
                    if (strcmp(attributes[j].attrName, lhs) == 0) {
                        if (attributes[j].indexNo != -1) {
                            indexExists = true;
                            indexCondition = i;
                            break;
                        }
                    }
                }
            }
        }

        // Declare a scan operator
        shared_ptr<QL_Op> scanOp;

        // If index exists
        if (indexExists) {
            // Get the index attribute information
            char* lhsRelName = (conditions[indexCondition].lhsAttr).relName;
            char* lhsAttrName = (conditions[indexCondition].lhsAttr).attrName;
            CompOp op = conditions[indexCondition].op;
            DataAttrInfo* attributeData = new DataAttrInfo;
            if ((rc = GetAttrInfoFromArray((char*) attributes, attrCount, lhsRelName, lhsAttrName, (char*) attributeData))) {
                return rc;
            }

            // Open the RM file
            RM_FileHandle rmFH;
            RID rid;
            RM_Record rec;
            char* recordData;
            if ((rc = rmManager->OpenFile(relName, rmFH))) {
                return rc;
            }

            // Open the index scan
            scanOp.reset(new QL_IndexScanOp(smManager, ixManager, rmManager, relName, attributeData->attrName, op, &conditions[indexCondition].rhsValue));
            if ((rc = scanOp->Open())) {
                return rc;
            }

            // Open all the indexes
            IX_IndexHandle* ixIHs = new IX_IndexHandle[attrCount];
            for (int i=0; i<attrCount; i++) {
                if (attributes[i].indexNo != -1) {
                    if ((rc = ixManager->OpenIndex(relName, i, ixIHs[i]))) {
                        return rc;
                    }
                }
            }

            // Find the entries to delete
            while ((rc = scanOp->GetNext(rid)) != QL_EOF) {
                // Get the record from the file
                if ((rc = rmFH.GetRec(rid, rec))) {
                    return rc;
                }
                if ((rc = rec.GetData(recordData))) {
                    return rc;
                }

                // Check the conditions
                bool match = true;
                if ((rc = CheckConditionsSingleRelation(recordData, match, (char*) attributes, attrCount, nConditions, conditions))) {
                    return rc;
                }

                // If all the conditions are satisfied
                if (match) {
                    // Delete the tuple
                    if ((rc = rmFH.DeleteRec(rid))) {
                        return rc;
                    }

                    // Delete entries from all indexes
                    for (int i=0; i<attrCount; i++) {
                        if (attributes[i].indexNo != -1) {
                            if ((rc = ixIHs[i].DeleteEntry(recordData + attributes[i].offset, rid))) {
                                return rc;
                            }
                        }
                    }

                    // Print the deleted tuple
                    p.Print(cout, recordData);
                }
            }

            // Close all the indexes
            for (int i=0; i<attrCount; i++) {
                if (attributes[i].indexNo != -1) {
                    if ((rc = ixManager->CloseIndex(ixIHs[i]))) {
                        return rc;
                    }
                }
            }
            delete[] ixIHs;
            delete attributeData;

            // Close the scan and RM file
            if ((rc = scanOp->Close())) {
                return rc;
            }
            if ((rc = rmManager->CloseFile(rmFH))) {
                return rc;
            }
        }

        // Else if no index
        else {
            // Open the RM file
            RM_FileHandle rmFH;
            RID rid;
            RM_Record rec;
            char* recordData;
            if ((rc = rmManager->OpenFile(relName, rmFH))) {
                return rc;
            }

            // Open a file scan on the first condition of the type R.A = v
            bool conditionExists = false;
            int conditionNumber = -1;
            for (int i=0; i<nConditions; i++) {
                Condition currentCondition = conditions[i];
                if (!currentCondition.bRhsIsAttr) {
                    conditionExists = true;
                    conditionNumber = i;
                    break;
                }
            }

            // If such a condition exists
            if (conditionExists) {
                // Open a file scan on that condition
                char* lhsRelName = (conditions[conditionNumber].lhsAttr).relName;
                char* lhsAttrName = (conditions[conditionNumber].lhsAttr).attrName;
                CompOp op = conditions[conditionNumber].op;

                DataAttrInfo* attributeData = new DataAttrInfo;
                if ((rc = GetAttrInfoFromArray((char*) attributes, attrCount, lhsRelName, lhsAttrName, (char*) attributeData))) {
                    return rc;
                }

                scanOp.reset(new QL_FileScanOp(smManager, rmManager, relName, true, attributeData->attrName, op, &conditions[conditionNumber].rhsValue));
                if ((rc = scanOp->Open())) {
                    return rc;
                }

                delete attributeData;
            }

            // Else if no such condition
            else {
                // Open a full file scan
                scanOp.reset(new QL_FileScanOp(smManager, rmManager, relName, false, NULL, NO_OP, NULL));
                if ((rc = scanOp->Open())) {
                    return rc;
                }
            }

            // Open all the indexes
            IX_IndexHandle* ixIHs = new IX_IndexHandle[attrCount];
            for (int i=0; i<attrCount; i++) {
                if (attributes[i].indexNo != -1) {
                    if ((rc = ixManager->OpenIndex(relName, i, ixIHs[i]))) {
                        return rc;
                    }
                }
            }

            // Get the next record to delete
            while ((rc = scanOp->GetNext(rid)) != QL_EOF) {
                if ((rc = rmFH.GetRec(rid, rec))) {
                    return rc;
                }
                if ((rc = rec.GetData(recordData))) {
                    return rc;
                }

                // Check the conditions
                bool match = true;
                if ((rc = CheckConditionsSingleRelation(recordData, match, (char*) attributes, attrCount, nConditions, conditions))) {
                    return rc;
                }

                // If all conditions are satisfied
                if (match) {
                    // Delete the tuple
                    if ((rc = rmFH.DeleteRec(rid))) {
                        return rc;
                    }

                    // Delete entries from all indexes
                    for (int i=0; i<attrCount; i++) {
                        if (attributes[i].indexNo != -1) {
                            if ((rc = ixIHs[i].DeleteEntry(recordData + attributes[i].offset, rid))) {
                                return rc;
                            }
                        }
                    }

                    // Print the deleted tuple
                    p.Print(cout, recordData);
                }
            }

            // Close all the indexes
            for (int i=0; i<attrCount; i++) {
                if (attributes[i].indexNo != -1) {
                    if ((rc = ixManager->CloseIndex(ixIHs[i]))) {
                        return rc;
                    }
                }
            }
            delete[] ixIHs;

            // Close the file scan and RM file
            if ((rc = scanOp->Close())) {
                return rc;
            }
            if ((rc = rmManager->CloseFile(rmFH))) {
                return rc;
            }
        }

        // Print the footer
        p.PrintFooter(cout);

        // Print the query plan
        if (bQueryPlans) {
            cout << "\nPhysical Query Plan :" << endl;
            cout << "DeleteOp (" << relName << ")" << endl;
            cout << "[" << endl;
            scanOp->Print(1);
            cout << "]" << endl;
        }
    }

    // Clean up
    delete rcRecord;
    delete[] attributes;

    // Return OK
    return OK_RC;
}


/************ UPDATE ************/

// Method: Update(const char *relName, const RelAttr &updAttr, const int bIsValue,
//                const RelAttr &rhsRelAttr, const Value &rhsValue, int nConditions,
//                const Condition conditions[])
// Update from the relName all tuples that satisfy conditions
/* Steps:
    1) Check the parameters
    2) Check whether the database is open
    3) Obtain attribute information for the relation and check
    4) Check the update attribute
    5) Check the conditions
    6) Find index on some condition
    7) If index exists
        - Open index scan
        - Find tuples and update
        - Update index entries
        - Close index scan
    8) If no index
        - Open file scan
        - Find tuples and update
        - Update index entries
        - Close file scan
    9) Print the updated tuples
*/
RC QL_Manager::Update(const char *relName,
                      const RelAttr &updAttr,
                      const int bIsValue,
                      const RelAttr &rhsRelAttr,
                      const Value &rhsValue,
                      int nConditions, const Condition conditions[]) {
        // Check the parameters
    if (relName == NULL) {
        return QL_NULL_RELATION;
    }

    // Check whether database is open
    if (!smManager->getOpenFlag()) {
        return QL_DATABASE_CLOSED;
    }

    if (strcmp(relName, "relcat") == 0 || strcmp(relName, "attrcat") == 0) {
        return QL_SYSTEM_CATALOG;
    }

    // Get the relation and attributes information
    int rc;
    SM_RelcatRecord* rcRecord = new SM_RelcatRecord;
    memset(rcRecord, 0, sizeof(SM_RelcatRecord));
    if ((rc = smManager->GetRelInfo(relName, rcRecord))) {
        delete rcRecord;
        return rc;
    }
    int attrCount = rcRecord->attrCount;
    int distributedRelation = rcRecord->distributed;
    char partitionAttrName[MAXNAME+1];
    strcpy(partitionAttrName, rcRecord->attrName);

    DataAttrInfo* attributes = new DataAttrInfo[attrCount];
    if ((rc = smManager->GetAttrInfo(relName, attrCount, (char*) attributes))) {
        return rc;
    }

    // Check the update attribute
    // Check the LHS
    char* updAttrRelName = updAttr.relName;
    if (updAttrRelName != NULL && strcmp(updAttrRelName, relName) != 0) {
        delete rcRecord;
        delete[] attributes;
        return QL_INVALID_UPDATE_ATTRIBUTE;
    }

    char* updAttrName = updAttr.attrName;
    AttrType updAttrType;
    bool found = false;
    for (int j=0; j<attrCount; j++) {
        if (strcmp(attributes[j].attrName, updAttrName) == 0) {
            updAttrType = attributes[j].attrType;
            found = true;
            break;
        }
    }
    if (!found) {
        delete rcRecord;
        delete[] attributes;
        return QL_INVALID_UPDATE_ATTRIBUTE;
    }

    // Check the RHS
    if (bIsValue) {
        AttrType valueType = rhsValue.type;
        if (valueType != updAttrType) {
            delete rcRecord;
            delete[] attributes;
            return QL_INVALID_UPDATE_ATTRIBUTE;
        }
    }
    else {
        char* valueRelName = rhsRelAttr.relName;
        if (valueRelName != NULL && strcmp(valueRelName, relName) != 0) {
            delete rcRecord;
            delete[] attributes;
            return QL_INVALID_UPDATE_ATTRIBUTE;
        }
        char* valueAttrName = rhsRelAttr.attrName;
        bool found = false;
        for (int j=0; j<attrCount; j++) {
            if (strcmp(attributes[j].attrName, valueAttrName) == 0) {
                if (attributes[j].attrType == updAttrType) {
                    found = true;
                    break;
                }
            }
        }
        if (!found) {
            delete rcRecord;
            delete[] attributes;
            return QL_INVALID_UPDATE_ATTRIBUTE;
        }
    }

    // Validate the conditions
    if ((rc = ValidateConditionsSingleRelation(relName, attrCount, (char*) attributes, nConditions, conditions))) {
        delete rcRecord;
        delete[] attributes;
        return rc;
    }

    // Print the command
    if (smManager->getPrintFlag()) {
        int i;
        cout << "Update\n";
        cout << "   relName = " << relName << "\n";
        cout << "   updAttr:" << updAttr << "\n";
        if (bIsValue)
            cout << "   rhs is value: " << rhsValue << "\n";
        else
            cout << "   rhs is attribute: " << rhsRelAttr << "\n";
        cout << "   nConditions = " << nConditions << "\n";
        for (i = 0; i < nConditions; i++)
            cout << "   conditions[" << i << "]:" << conditions[i] << "\n";
    }

    // EX - Distributed relation case
    if (distributedRelation) {
        // Check whether the partition attribute is used in a condition
        bool condExists = false;
        int conditionNumber = -1;
        for (int i=0; i<nConditions; i++) {
            Condition currentCondition = conditions[i];
            char* lhs = (currentCondition.lhsAttr).attrName;
            int rhsIsAttr = currentCondition.bRhsIsAttr;
            if (!rhsIsAttr) {
                if (strcmp(lhs, partitionAttrName) == 0) {
                    condExists = true;
                    conditionNumber = i;
                    break;
                }
            }
        }

        // If condition on partition attribute exists
        bool reshuffle = (strcmp(partitionAttrName, updAttr.attrName) == 0);
        int numberNodes = smManager->getNumberNodes();
        EX_CommLayer commLayer(rmManager, ixManager);
        if (condExists) {
            // Find the nodes to pass the query
            for (int i=1; i<=numberNodes; i++) {
                bool valid = false;
                if ((rc = CheckDataNodeForCondition(rmManager, relName, partitionAttrName, conditions[conditionNumber], i, valid))) {
                    return rc;
                }
                if (valid) {
                    // Pass the query to the node
                    if ((rc = commLayer.UpdateInDataNode(relName, updAttr, bIsValue, rhsRelAttr, rhsValue, nConditions, conditions, i, reshuffle))) {
                        return rc;
                    }
                }
            }
        }

        // Else if no such condition
        else {
            // Pass the query to all data nodes
            for (int i=1; i<=numberNodes; i++) {
                if ((rc = commLayer.UpdateInDataNode(relName, updAttr, bIsValue, rhsRelAttr, rhsValue, nConditions, conditions, i, reshuffle))) {
                    return rc;
                }
            }
        }

        // If the partitioned attribute is updated, print message
        if (reshuffle) {
            cout << "\n* Reshuffling data *" << endl;
        }
    }

    else {
        // Prepare the printer class
        cout << "Updated tuples:" << endl;
        Printer p(attributes, attrCount);
        p.PrintHeader(cout);

        // Find whether index exists on some condition
        bool indexExists = false;
        int indexCondition = -1;
        for (int i=0; i<nConditions; i++) {
            if (indexExists) break;
            Condition currentCondition = conditions[i];
            char* lhs = (currentCondition.lhsAttr).attrName;
            int rhsIsAttr = currentCondition.bRhsIsAttr;
            if (!rhsIsAttr) {
                for (int j=0; j<attrCount; j++) {
                    if (strcmp(attributes[j].attrName, lhs) == 0) {
                        // Do not consider an index on the update attribute
                        if (strcmp(attributes[j].attrName, updAttrName) != 0 && attributes[j].indexNo != -1) {
                            indexExists = true;
                            indexCondition = i;
                            break;
                        }
                    }
                }
            }
        }

        // Create a QL_Op object
        shared_ptr<QL_Op> scanOp;

        // If index exists
        if (indexExists) {
            // Get the index attribute information
            char* lhsRelName = (conditions[indexCondition].lhsAttr).relName;
            char* lhsAttrName = (conditions[indexCondition].lhsAttr).attrName;
            CompOp op = conditions[indexCondition].op;
            DataAttrInfo* attributeData = new DataAttrInfo;
            if ((rc = GetAttrInfoFromArray((char*) attributes, attrCount, lhsRelName, lhsAttrName, (char*) attributeData))) {
                return rc;
            }

            // Open the RM file
            RM_FileHandle rmFH;
            RID rid;
            RM_Record rec;
            char* recordData;
            if ((rc = rmManager->OpenFile(relName, rmFH))) {
                return rc;
            }

            // Open the IndexScan operator
            scanOp.reset(new QL_IndexScanOp(smManager, ixManager, rmManager, relName, attributeData->attrName, op, &conditions[indexCondition].rhsValue));
            if ((rc = scanOp->Open())) {
                return rc;
            }

            // Open the update attribute index if it exists
            IX_IndexHandle updAttrIH;
            DataAttrInfo* updAttrData = new DataAttrInfo;
            if ((rc = GetAttrInfoFromArray((char*) attributes, attrCount, updAttrRelName, updAttrName, (char*) updAttrData))) {
                return rc;
            }
            if (updAttrData->indexNo != -1) {
                if ((rc = ixManager->OpenIndex(relName, updAttrData->indexNo, updAttrIH))) {
                    return rc;
                }
            }

            // Find the entries to update
            while ((rc = scanOp->GetNext(rid)) != QL_EOF) {
                // Return if some error
                if (rc) {
                    return rc;
                }

                // Get the record from the file
                if ((rc = rmFH.GetRec(rid, rec))) {
                    return rc;
                }
                if ((rc = rec.GetData(recordData))) {
                    return rc;
                }

                // Check the conditions
                bool match = true;
                if ((rc = CheckConditionsSingleRelation(recordData, match, (char*) attributes, attrCount, nConditions, conditions))) {
                    return rc;
                }

                // If all the conditions are satisfied
                if (match) {
                    // Delete the index entry if it exists
                    if (updAttrData->indexNo != -1) {
                        if ((rc = updAttrIH.DeleteEntry(recordData + updAttrData->offset, rid))) {
                            return rc;
                        }
                    }

                    // Update the record data
                    // If RHS is a value
                    if (bIsValue) {
                        if (updAttrType == INT) {
                            int value = *static_cast<int*>(rhsValue.data);
                            memcpy(recordData + updAttrData->offset, &value, sizeof(value));
                        }
                        else if (updAttrType == FLOAT) {
                            float value = *static_cast<float*>(rhsValue.data);
                            memcpy(recordData + updAttrData->offset, &value, sizeof(value));
                        }
                        else {
                            char* value = static_cast<char*>(rhsValue.data);
                            memcpy(recordData + updAttrData->offset, value, updAttrData->attrLength);
                        }
                    }

                    // Else if RHS is an attribute
                    else {
                        DataAttrInfo* rhsAttrData = new DataAttrInfo;
                        if ((rc = GetAttrInfoFromArray((char*) attributes, attrCount, rhsRelAttr.relName, rhsRelAttr.attrName, (char*) rhsAttrData))) {
                            return rc;
                        }
                        memcpy(recordData + updAttrData->offset, recordData + rhsAttrData->offset, updAttrData->attrLength);
                        delete rhsAttrData;
                    }

                    // Update the record in the file
                    if ((rc = rmFH.UpdateRec(rec))) {
                        return rc;
                    }

                    // Update entry in the index if it exists
                    if (updAttrData->indexNo != -1) {
                        if ((rc = updAttrIH.InsertEntry(recordData + updAttrData->offset, rid))) {
                            return rc;
                        }
                    }

                    // Print the deleted tuple
                    p.Print(cout, recordData);
                }
            }

            // Close the update attribute index if it exists
            if (updAttrData->indexNo != -1) {
                if ((rc = ixManager->CloseIndex(updAttrIH))) {
                    return rc;
                }
            }

            // Clean up
            delete attributeData;
            delete updAttrData;

            // Close the scan and file
            if ((rc = scanOp->Close())) {
                return rc;
            }
            if ((rc = rmManager->CloseFile(rmFH))) {
                return rc;
            }
        }

        // Else if no index
        else {
            // Open the RM file
            RM_FileHandle rmFH;
            RID rid;
            RM_Record rec;
            char* recordData;
            if ((rc = rmManager->OpenFile(relName, rmFH))) {
                return rc;
            }

            // Open a file scan on the first condition of the type R.A = v
            bool conditionExists = false;
            int conditionNumber = -1;
            for (int i=0; i<nConditions; i++) {
                Condition currentCondition = conditions[i];
                if (!currentCondition.bRhsIsAttr) {
                    conditionExists = true;
                    conditionNumber = i;
                    break;
                }
            }

            // If such a condition exists
            if (conditionExists) {
                // Open a file scan on that condition
                char* lhsRelName = (conditions[conditionNumber].lhsAttr).relName;
                char* lhsAttrName = (conditions[conditionNumber].lhsAttr).attrName;
                CompOp op = conditions[conditionNumber].op;

                DataAttrInfo* attributeData = new DataAttrInfo;
                if ((rc = GetAttrInfoFromArray((char*) attributes, attrCount, lhsRelName, lhsAttrName, (char*) attributeData))) {
                    return rc;
                }

                // Open a conditional file scan
                scanOp.reset(new QL_FileScanOp(smManager, rmManager, relName, true, attributeData->attrName, op, &conditions[conditionNumber].rhsValue));
                if ((rc = scanOp->Open())) {
                    return rc;
                }
                delete attributeData;
            }

            // Else if no such condition
            else {
                // Open a full file scan
                scanOp.reset(new QL_FileScanOp(smManager, rmManager, relName, false, NULL, NO_OP, NULL));
                if ((rc = scanOp->Open())) {
                    return rc;
                }
            }

            // Open the update attribute index if it exists
            IX_IndexHandle ixIH;
            DataAttrInfo* updAttrData = new DataAttrInfo;
            if ((rc = GetAttrInfoFromArray((char*) attributes, attrCount, updAttrRelName, updAttrName, (char*) updAttrData))) {
                return rc;
            }
            if (updAttrData->indexNo != -1) {
                if ((rc = ixManager->OpenIndex(relName, updAttrData->indexNo, ixIH))) {
                    return rc;
                }
            }

            // Get the next record to update
            while ((rc = scanOp->GetNext(rid)) != QL_EOF) {
                if ((rc = rmFH.GetRec(rid, rec))) {
                    return rc;
                }

                if ((rc = rec.GetData(recordData))) {
                    return rc;
                }

                // Check the conditions
                bool match = true;
                if ((rc = CheckConditionsSingleRelation(recordData, match, (char*) attributes, attrCount, nConditions, conditions))) {
                    return rc;
                }

                // If all conditions are satisfied
                if (match) {
                    // Delete the index entry if it exists
                    if (updAttrData->indexNo != -1) {
                        if ((rc = ixIH.DeleteEntry(recordData + updAttrData->offset, rid))) {
                            return rc;
                        }
                    }

                    // Update the record data
                    // If RHS is a value
                    if (bIsValue) {
                        if (updAttrType == INT) {
                            int value = *static_cast<int*>(rhsValue.data);
                            memcpy(recordData + updAttrData->offset, &value, sizeof(value));
                        }
                        else if (updAttrType == FLOAT) {
                            float value = *static_cast<float*>(rhsValue.data);
                            memcpy(recordData + updAttrData->offset, &value, sizeof(value));
                        }
                        else {
                            char* value = static_cast<char*>(rhsValue.data);
                            memcpy(recordData + updAttrData->offset, value, updAttrData->attrLength);
                        }
                    }

                    // Else if RHS is an attribute
                    else {
                        DataAttrInfo* rhsAttrData = new DataAttrInfo;
                        if ((rc = GetAttrInfoFromArray((char*) attributes, attrCount, rhsRelAttr.relName, rhsRelAttr.attrName, (char*) rhsAttrData))) {
                            return rc;
                        }
                        memcpy(recordData + updAttrData->offset, recordData + rhsAttrData->offset, updAttrData->attrLength);
                        delete rhsAttrData;
                    }

                    // Update the record in the file
                    if ((rc = rmFH.UpdateRec(rec))) {
                        return rc;
                    }

                    // Update entry in the index if it exists
                    if (updAttrData->indexNo != -1) {
                        if ((rc = ixIH.InsertEntry(recordData + updAttrData->offset, rid))) {
                            return rc;
                        }
                    }

                    // Print the updated tuple
                    p.Print(cout, recordData);
                }
            }

            // Close the open index if any
            if (updAttrData->indexNo != -1) {
                if ((rc = ixManager->CloseIndex(ixIH))) {
                    return rc;
                }
            }
            delete updAttrData;

            // Close the file scan
            if ((rc = scanOp->Close())) {
                return rc;
            }

            // Close the RM file
            if ((rc = rmManager->CloseFile(rmFH))) {
                return rc;
            }
        }

        // Print the footer
        p.PrintFooter(cout);

        // Print the query plan
        if (bQueryPlans) {
            cout << "\nPhysical Query Plan :" << endl;
            cout << "UpdateOp (" << relName << "." << updAttrName << ")" << endl;
            cout << "[" << endl;
            scanOp->Print(1);
            cout << "]" << endl;
        }
    }

    // Clean up
    delete rcRecord;
    delete[] attributes;

    // Return OK
    return OK_RC;
}


/************ HELPER METHODS ************/

// Method: ValidateConditionsSingleRelation(const char* relName, int attrCount, char* attributeData, int nConditions, const Condition conditions[])
// Validate the conditions for a single relation
RC QL_Manager::ValidateConditionsSingleRelation(const char* relName, int attrCount, char* attributeData, int nConditions, const Condition conditions[]) {
    DataAttrInfo* attributes = (DataAttrInfo*) attributeData;
    for (int i=0; i<nConditions; i++) {
        Condition currentCondition = conditions[i];

        // Check whether LHS is a correct attribute
        char* lhsRelName = (currentCondition.lhsAttr).relName;
        if (lhsRelName != NULL && strcmp(lhsRelName, relName) != 0) {
            return QL_INVALID_CONDITION;
        }

        char* lhs = (currentCondition.lhsAttr).attrName;
        AttrType lhsType;
        bool found = false;
        for (int j=0; j<attrCount; j++) {
            if (strcmp(attributes[j].attrName, lhs) == 0) {
                lhsType = attributes[j].attrType;
                found = true;
                break;
            }
        }
        if (!found) {
            return QL_INVALID_CONDITION;
        }

        // Check if rhs is of correct type
        if (currentCondition.bRhsIsAttr) {
            char* rhsRelName = (currentCondition.rhsAttr).relName;
            if (rhsRelName != NULL && strcmp(rhsRelName, relName) != 0) {
                return QL_INVALID_CONDITION;
            }
            char* rhs = (currentCondition.rhsAttr).attrName;
            bool found = false;
            for (int j=0; j<attrCount; j++) {
                if (strcmp(attributes[j].attrName, rhs) == 0) {
                    if (attributes[j].attrType == lhsType) {
                        found = true;
                        break;
                    }
                }
            }
            if (!found) {
                return QL_INVALID_CONDITION;
            }
        }
        else {
            AttrType rhsType = (currentCondition.rhsValue).type;
            if (rhsType != lhsType) {
                return QL_INVALID_CONDITION;
            }
        }
    }

    return OK_RC;
}

// Method: CheckConditionsSingleRelation(char* recordData, bool& match, char* attributeData, int attrCount, int nConditions, const Condition conditions[])
// Check the conditions for a single relation
RC QL_Manager::CheckConditionsSingleRelation(char* recordData, bool& match, char* attributeData, int attrCount, int nConditions, const Condition conditions[]) {
    int rc;
    DataAttrInfo* lhsData = new DataAttrInfo;
    DataAttrInfo* rhsData = new DataAttrInfo;
    for (int i=0; i<nConditions; i++) {
        Condition currentCondition = conditions[i];
        char* lhsRelName = (currentCondition.lhsAttr).relName;
        char* lhsAttrName = (currentCondition.lhsAttr).attrName;
        if ((rc = GetAttrInfoFromArray(attributeData, attrCount, lhsRelName, lhsAttrName, (char*) lhsData))) {
            return rc;
        }

        // If the RHS is also an attribute
        if (currentCondition.bRhsIsAttr) {
            char* rhsRelName = (currentCondition.rhsAttr).relName;
            char* rhsAttrName = (currentCondition.rhsAttr).attrName;
            if ((rc = GetAttrInfoFromArray(attributeData, attrCount, rhsRelName, rhsAttrName, (char*) rhsData))) {
                return rc;
            }

            if (lhsData->attrType == INT) {
                int lhsValue, rhsValue;
                memcpy(&lhsValue, recordData + (lhsData->offset), sizeof(lhsValue));
                memcpy(&rhsValue, recordData + (rhsData->offset), sizeof(rhsValue));
                if (!matchRecord(lhsValue, rhsValue, currentCondition.op)) {
                    match = false;
                    break;
                }
            }
            else if (lhsData->attrType == FLOAT) {
                float lhsValue, rhsValue;
                memcpy(&lhsValue, recordData + (lhsData->offset), sizeof(lhsValue));
                memcpy(&rhsValue, recordData + (rhsData->offset), sizeof(rhsValue));
                if (!matchRecord(lhsValue, rhsValue, currentCondition.op)) {
                    match = false;
                    break;
                }
            }
            else {
                string lhsValue(recordData + (lhsData->offset));
                string rhsValue(recordData + (rhsData->offset));
                if (!matchRecord(lhsValue, rhsValue, currentCondition.op)) {
                    match = false;
                    break;
                }
            }
        }

        // Else if the RHS is a constant value
        else {
            if (lhsData->attrType == INT) {
                int lhsValue;
                memcpy(&lhsValue, recordData + (lhsData->offset), sizeof(lhsValue));
                int rhsValue = *static_cast<int*>((currentCondition.rhsValue).data);
                if (!matchRecord(lhsValue, rhsValue, currentCondition.op)) {
                    match = false;
                    break;
                }
            }
            else if (lhsData->attrType == FLOAT) {
                float lhsValue;
                memcpy(&lhsValue, recordData + (lhsData->offset), sizeof(lhsValue));
                float rhsValue = *static_cast<float*>((currentCondition.rhsValue).data);
                if (!matchRecord(lhsValue, rhsValue, currentCondition.op)) {
                    match = false;
                    break;
                }
            }
            else {
                string lhsValue(recordData + (lhsData->offset));
                char* rhsValueChar = static_cast<char*>((currentCondition.rhsValue).data);
                string rhsValue(rhsValueChar);
                if (!matchRecord(lhsValue, rhsValue, currentCondition.op)) {
                    match = false;
                    break;
                }
            }
        }
    }

    // Clean up
    delete lhsData;
    delete rhsData;

    return OK_RC;
}

// Method: ValidateConditionsMultipleRelations(SM_RelcatRecord* rcRecords[], char* attributeData[], int nRelations, int nConditions, Condition conditions[])
// Validate the conditions for multiple relations
RC QL_Manager::ValidateConditionsMultipleRelations(SM_RelcatRecord* rcRecords[], char* attributeData[], int nRelations, int nConditions, Condition conditions[]) {
    DataAttrInfo** attributes = (DataAttrInfo**) attributeData;
    for (int i=0; i<nConditions; i++) {
        Condition currentCondition = conditions[i];

        // Check whether LHS is a valid attribute
        char* lhsRelName = (currentCondition.lhsAttr).relName;
        int relationNumber = -1;
        bool found = false;
        if (lhsRelName == NULL) {
            found = true;
        }
        else {
            for (int j=0; j<nRelations; j++) {
                if (lhsRelName == NULL || strcmp(lhsRelName, rcRecords[j]->relName) == 0) {
                    relationNumber = j;
                    found = true;
                    break;
                }
            }
        }
        if (!found) {
            return QL_INVALID_CONDITION;
        }

        char* lhsAttrName = (currentCondition.lhsAttr).attrName;
        AttrType lhsType;
        found = false;

        // If no relation name specified
        if (lhsRelName == NULL) {
            for (int k=0; k<nRelations; k++) {
                for (int j=0; j<rcRecords[k]->attrCount; j++) {
                    if (strcmp(attributes[k][j].attrName, lhsAttrName) == 0) {
                        lhsType = attributes[k][j].attrType;
                        (conditions[i].lhsAttr).relName = rcRecords[k]->relName;
                        found = true;
                        break;
                    }
                }
                if (found) break;
            }
        }

        // Else if relation name is specified
        else {
            for (int j=0; j<rcRecords[relationNumber]->attrCount; j++) {
                if (strcmp(attributes[relationNumber][j].attrName, lhsAttrName) == 0) {
                    lhsType = attributes[relationNumber][j].attrType;
                    found = true;
                    break;
                }
            }
        }

        if (!found) {
            return QL_INVALID_CONDITION;
        }

        // Check if rhs is of correct type
        if (currentCondition.bRhsIsAttr) {
            char* rhsRelName = (currentCondition.rhsAttr).relName;

            // Check the relation name
            relationNumber = -1;
            found = false;
            if (lhsRelName == NULL) {
                found = true;
            }
            else {
                for (int j=0; j<nRelations; j++) {
                    if (strcmp(rhsRelName, rcRecords[j]->relName) == 0) {
                        relationNumber = j;
                        found = true;
                        break;
                    }
                }
            }
            if (!found) {
                return QL_INVALID_CONDITION;
            }

            // Check the attribute name
            char* rhsAttrName = (currentCondition.rhsAttr).attrName;
            found = false;
            AttrType rhsType;

            // If no relation name specified
            if (rhsRelName == NULL) {
                for (int k=0; k<nRelations; k++) {
                    for (int j=0; j<rcRecords[k]->attrCount; j++) {
                        if (strcmp(attributes[k][j].attrName, rhsAttrName) == 0) {
                            rhsType = attributes[k][j].attrType;
                            (conditions[i].rhsAttr).relName = rcRecords[k]->relName;
                            found = true;
                            break;
                        }
                    }
                    if (found) break;
                }
            }

            // Else if relation name is specified
            else {
                for (int j=0; j<rcRecords[relationNumber]->attrCount; j++) {
                    if (strcmp(attributes[relationNumber][j].attrName, rhsAttrName) == 0) {
                        rhsType = attributes[relationNumber][j].attrType;
                        found = true;
                        break;
                    }
                }
            }
            if (!found || lhsType != rhsType) {
                return QL_INVALID_CONDITION;
            }
        }
        else {
            AttrType rhsType = (currentCondition.rhsValue).type;
            if (rhsType != lhsType) {
                // String to int type conversion
                if (lhsType == INT && rhsType == STRING) {
                    (conditions[i].rhsValue).type = INT;
                    int value = atoi(static_cast<char*>((currentCondition.rhsValue).data));
                    memcpy((conditions[i].rhsValue).data, &value, sizeof(value));
                }
                // String to float type conversion
                else if (lhsType == FLOAT && rhsType == STRING) {
                    (conditions[i].rhsValue).type = FLOAT;
                    float value = atof(static_cast<char*>((currentCondition.rhsValue).data));
                    memcpy((conditions[i].rhsValue).data, &value, sizeof(value));
                }
                // Float to int type conversion
                else if (lhsType == INT && rhsType == FLOAT) {
                    (conditions[i].rhsValue).type = INT;
                    int value = (int) (*static_cast<float*>((currentCondition.rhsValue).data));
                    memcpy((conditions[i].rhsValue).data, &value, sizeof(value));
                }
                // Int to float type conversion
                else if (lhsType == FLOAT && rhsType == INT) {
                    (conditions[i].rhsValue).type = FLOAT;
                    float value = (float) (*static_cast<int*>((currentCondition.rhsValue).data));
                    memcpy((conditions[i].rhsValue).data, &value, sizeof(value));
                }
                else {
                    return QL_INVALID_CONDITION;
                }
            }
        }
    }

    return OK_RC;
}


// Method: RemoveCondition(Condition conditions[], int nConditions, int index)
// Remove a condition from the conditions array
void RemoveCondition(Condition conditions[], int &nConditions, int index) {
    for (int i=index; i<nConditions-1; i++) {
        conditions[i] = conditions[i+1];
    }
    nConditions--;
}
