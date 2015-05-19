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
#include "redbase.h"
#include "sm.h"
#include "ql.h"
#include "ix.h"
#include "rm.h"
#include "printer.h"
#include "parser.h"

using namespace std;

//
// QL_Manager::QL_Manager(SM_Manager &smm, IX_Manager &ixm, RM_Manager &rmm)
//
// Constructor for the QL Manager
//
QL_Manager::QL_Manager(SM_Manager &smm, IX_Manager &ixm, RM_Manager &rmm) {
    // Store the objects
    this->rmManager = &rmm;
    this->ixManager = &ixm;
    this->smManager = &smm;
}

//
// QL_Manager::~QL_Manager()
//
// Destructor for the QL Manager
//
QL_Manager::~QL_Manager() {
    // Nothing to free
}


/************ SELECT ************/

//
// Handle the select clause
//
RC QL_Manager::Select(int nSelAttrs, const RelAttr selAttrs[],
                      int nRelations, const char * const relations[],
                      int nConditions, const Condition conditions[]) {
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

    return 0;
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

    // Clean up
    delete rcRecord;
    delete[] attributes;
    delete[] tupleData;
    delete[] ixIH;

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

    // Print the command
    if (smManager->getPrintFlag()) {
        int i;
        cout << "Delete\n";
        cout << "   relName = " << relName << "\n";
        cout << "   nCondtions = " << nConditions << "\n";
        for (i = 0; i < nConditions; i++)
            cout << "   conditions[" << i << "]:" << conditions[i] << "\n";
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
    DataAttrInfo* attributes = new DataAttrInfo[attrCount];
    if ((rc = smManager->GetAttrInfo(relName, attrCount, (char*) attributes))) {
        return rc;
    }
    char* tupleData = new char[tupleLength];
    memset(tupleData, 0, tupleLength);

    // Check the conditions
    for (int i=0; i<nConditions; i++) {
        Condition currentCondition = conditions[i];

        // Check whether LHS is a correct attribute
        char* lhsRelName = (currentCondition.lhsAttr).relName;
        if (lhsRelName != NULL && strcmp(lhsRelName, relName) != 0) {
            delete rcRecord;
            delete[] attributes;
            delete[] tupleData;
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
            delete rcRecord;
            delete[] attributes;
            delete[] tupleData;
            return QL_INVALID_CONDITION;
        }

        // Check if rhs is of correct type
        if (currentCondition.bRhsIsAttr) {
            char* rhsRelName = (currentCondition.rhsAttr).relName;
            if (rhsRelName != NULL && strcmp(rhsRelName, relName) != 0) {
                delete rcRecord;
                delete[] attributes;
                delete[] tupleData;
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
                delete rcRecord;
                delete[] attributes;
                delete[] tupleData;
                return QL_INVALID_CONDITION;
            }
        }
        else {
            AttrType rhsType = (currentCondition.rhsValue).type;
            if (rhsType != lhsType) {
                delete rcRecord;
                delete[] attributes;
                delete[] tupleData;
                return QL_INVALID_CONDITION;
            }
        }
    }

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

    // Prepare the printer class
    string queryPlan = "";
    cout << "Deleted tuples:" << endl;
    Printer p(attributes, attrCount);
    p.PrintHeader(cout);

    // If index exists
    if (indexExists) {
        // Get the index attribute information
        char* lhs = (conditions[indexCondition].lhsAttr).attrName;
        CompOp op = conditions[indexCondition].op;
        DataAttrInfo* attributeData = new DataAttrInfo;
        if ((rc = GetAttrInfoFromArray((char*) attributes, attrCount, lhs, (char*) attributeData))) {
            return rc;
        }
        int lhsIndexNo = attributeData->indexNo;

        // Open the RM file
        RM_FileHandle rmFH;
        IX_IndexHandle ixIH;
        IX_IndexScan ixIS;
        RID rid;
        RM_Record rec;
        char* recordData;
        if ((rc = rmManager->OpenFile(relName, rmFH))) {
            return rc;
        }

        // Open the index scan
        if ((rc = ixManager->OpenIndex(relName, lhsIndexNo, ixIH))) {
            return rc;
        }
        if ((rc = ixIS.OpenScan(ixIH, op, (conditions[indexCondition].rhsValue).data))) {
            return rc;
        }
        queryPlan += "IndexScan (";
        queryPlan += relName;
        queryPlan += ", ";
        queryPlan += attributeData->attrName;
        queryPlan += OperatorToString(op);
        GetValue(conditions[indexCondition].rhsValue, queryPlan);
        queryPlan += ")\n";

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
        while ((rc = ixIS.GetNextEntry(rid)) != IX_EOF) {
            // Get the record from the file
            if ((rc = rmFH.GetRec(rid, rec))) {
                return rc;
            }
            if ((rc = rec.GetData(recordData))) {
                return rc;
            }

            // Check the conditions
            bool match = true;
            DataAttrInfo* lhsData = new DataAttrInfo;
            DataAttrInfo* rhsData = new DataAttrInfo;
            for (int i=0; i<nConditions; i++) {
                Condition currentCondition = conditions[i];
                char* lhs = (currentCondition.lhsAttr).attrName;
                if ((rc = GetAttrInfoFromArray((char*) attributes, attrCount, lhs, (char*) lhsData))) {
                    return rc;
                }

                // If the RHS is also an attribute
                if (currentCondition.bRhsIsAttr) {
                    char* rhs = (currentCondition.rhsAttr).attrName;
                    if ((rc = GetAttrInfoFromArray((char*) attributes, attrCount, rhs, (char*) rhsData))) {
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

            delete lhsData;
            delete rhsData;
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

        // Close the scans and files
        if ((rc = ixIS.CloseScan())) {
            return rc;
        }
        if ((rc = ixManager->CloseIndex(ixIH))) {
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
        RM_FileScan rmFS;
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
            char* lhs = (conditions[conditionNumber].lhsAttr).attrName;
            CompOp op = conditions[conditionNumber].op;

            DataAttrInfo* attributeData = new DataAttrInfo;
            if ((rc = GetAttrInfoFromArray((char*) attributes, attrCount, lhs, (char*) attributeData))) {
                return rc;
            }
            AttrType lhsType = attributeData->attrType;
            int lhsLength = attributeData->attrLength;
            int lhsOffset = attributeData->offset;

            if ((rc = rmFS.OpenScan(rmFH, lhsType, lhsLength, lhsOffset, op, (conditions[conditionNumber].rhsValue).data))) {
                return rc;
            }

            queryPlan += "FileScan (";
            queryPlan += relName;
            queryPlan += ", ";
            queryPlan += attributeData->attrName;
            queryPlan += OperatorToString(op);
            GetValue(conditions[conditionNumber].rhsValue, queryPlan);
            queryPlan += ")\n";

            delete attributeData;
        }

        // Else if no such condition
        else {
            // Open a full file scan
            if ((rc = rmFS.OpenScan(rmFH, INT, 4, 0, NO_OP, NULL))) {
                return rc;
            }

            queryPlan += "FileScan (";
            queryPlan += relName;
            queryPlan += ")\n";
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
        while ((rc = rmFS.GetNextRec(rec)) != RM_EOF) {
            if ((rc = rec.GetData(recordData))) {
                return rc;
            }
            if ((rc = rec.GetRid(rid))) {
                return rc;
            }

            // Check the conditions
            bool match = true;
            DataAttrInfo* lhsData = new DataAttrInfo;
            DataAttrInfo* rhsData = new DataAttrInfo;
            for (int i=0; i<nConditions; i++) {
                Condition currentCondition = conditions[i];
                char* lhs = (currentCondition.lhsAttr).attrName;
                if ((rc = GetAttrInfoFromArray((char*) attributes, attrCount, lhs, (char*) lhsData))) {
                    return rc;
                }

                // If the RHS is also an attribute
                if (currentCondition.bRhsIsAttr) {
                    char* rhs = (currentCondition.rhsAttr).attrName;
                    if ((rc = GetAttrInfoFromArray((char*) attributes, attrCount, rhs, (char*) rhsData))) {
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

            delete lhsData;
            delete rhsData;
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

        // Close the file scan
        if ((rc = rmFS.CloseScan())) {
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
        cout << queryPlan;
    }

    // Clean up
    delete rcRecord;
    delete[] attributes;
    delete[] tupleData;

    // Return OK
    return OK_RC;
}


/************ UPDATE ************/

// Method: Update(const char *relName, const RelAttr &updAttr, const int bIsValue,
//                const RelAttr &rhsRelAttr, const Value &rhsValue, int nConditions,
//                const Condition conditions[])
// Update from the relName all tuples that satisfy conditions
/* Steps:

*/
RC QL_Manager::Update(const char *relName,
                      const RelAttr &updAttr,
                      const int bIsValue,
                      const RelAttr &rhsRelAttr,
                      const Value &rhsValue,
                      int nConditions, const Condition conditions[]) {
    // Check the parameters

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

    // Return OK
    return OK_RC;
}


// Method: GetAttrInfoFromArray(char* attributes, int attrCount, char* attributeData)
// Get the attribute info from the attributes array
RC QL_Manager::GetAttrInfoFromArray(char* attributes, int attrCount, const char* attrName, char* attributeData) {
    bool found = false;
    DataAttrInfo* attributesArray = (DataAttrInfo*) attributes;
    DataAttrInfo* attribute = (DataAttrInfo*) attributeData;
    for (int i=0; i<attrCount; i++) {
        if (strcmp(attributesArray[i].attrName, attrName) == 0) {
            strcpy(attribute->relName, attributesArray[i].relName);
            strcpy(attribute->attrName, attributesArray[i].attrName);
            attribute->offset = attributesArray[i].offset;
            attribute->attrType = attributesArray[i].attrType;
            attribute->attrLength = attributesArray[i].attrLength;
            attribute->indexNo = attributesArray[i].indexNo;
            found = true;
            break;
        }
    }

    if (!found) {
        return QL_ATTRIBUTE_NOT_FOUND;
    }

    return OK_RC;
}

// Method: matchRecord(T lhsValue, T rhsValue, CompOp op)
// Template function to compare two values
template <typename T>
bool QL_Manager::matchRecord(T lhsValue, T rhsValue, CompOp op) {
    bool recordMatch = false;
    switch(op) {
        case EQ_OP:
            if (lhsValue == rhsValue) recordMatch = true;
            break;
        case LT_OP:
            if (lhsValue < rhsValue) recordMatch = true;
            break;
        case GT_OP:
            if (lhsValue > rhsValue) recordMatch = true;
            break;
        case LE_OP:
            if (lhsValue <= rhsValue) recordMatch = true;
            break;
        case GE_OP:
            if (lhsValue >= rhsValue) recordMatch = true;
            break;
        case NE_OP:
            if (lhsValue != rhsValue) recordMatch = true;
            break;
        default:
            break;
    }
    return recordMatch;
}

// Get the string form of the operator
const char* QL_Manager::OperatorToString(CompOp op) {
    switch(op) {
        case EQ_OP:
            return " = ";
        case LT_OP:
            return " < ";
        case GT_OP:
            return " > ";
        case LE_OP:
            return " <= ";
        case GE_OP:
            return " >= ";
        case NE_OP:
            return " != ";
        default:
            return " NO_OP ";
    }
}

// Get the value in a string for printing
void QL_Manager::GetValue(Value v, string& queryPlan) {
    AttrType vType = v.type;
    if (vType == INT) {
        int value = *static_cast<int*>(v.data);
        stringstream ss;
        ss << value;
        queryPlan += ss.str();
    }
    else if (vType == FLOAT) {
        float value = *static_cast<float*>(v.data);
        stringstream ss;
        ss << value;
        queryPlan += ss.str();
    }
    else {
        char* value = static_cast<char*>(v.data);
        queryPlan += value;
    }
}
