//
// File:        sm_manager.cc
// Description: SM_Manager class implementation
// Authors:     Aditya Bhandari (adityasb@stanford.edu)
//

#include <cstdio>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include "redbase.h"
#include "sm.h"
#include "ix.h"
#include "rm.h"
#include "printer.h"
#include "parser.h"
using namespace std;

// Constructor
SM_Manager::SM_Manager(IX_Manager &ixm, RM_Manager &rmm) {
    // Store the RM and IX managers
    this->rmManager = &rmm;
    this->ixManager = &ixm;

    // Initialize the flag
    isOpen = FALSE;

    // Initialize the system parameters
    printCommands = FALSE;
    optimizeQuery = TRUE;
}

// Destructor
SM_Manager::~SM_Manager() {
    // Nothing to free
}


// Method: OpenDb(const char *dbName)
// Open the database
/* Steps:
    1) Check if the database is already open
    2) Change to the database directory
    3) Open the system catalogs
    4) Update flag
*/
RC SM_Manager::OpenDb(const char *dbName) {
    // Check if open
    if (isOpen) {
        return SM_DATABASE_OPEN;
    }

    // Check the parameter
    if (dbName == NULL) {
        return SM_INVALID_DATABASE_NAME;
    }

    // Change to the database directory
    if (chdir(dbName) == -1) {
        return SM_INVALID_DATABASE_NAME;
    }

    // Check whether the database is distributed
    int rc;
    RM_FileHandle dbInfoFH;
    RM_FileScan dbInfoFS;
    RM_Record rec;
    char* recordData;

    // Open the file and get data
    if ((rc = rmManager->OpenFile("dbinfo", dbInfoFH))) {
        return rc;
    }
    if ((rc = dbInfoFS.OpenScan(dbInfoFH, INT, 4, 0, NO_OP, NULL))) {
        return rc;
    }
    if ((rc = dbInfoFS.GetNextRec(rec))) {
        return rc;
    }
    if ((rc = rec.GetData(recordData))) {
        return rc;
    }
    EX_DBInfo* dbInfo = (EX_DBInfo*) recordData;
    this->distributed = dbInfo->distributed;
    this->numberNodes = dbInfo->numberNodes;

    // Close the file
    if ((rc = dbInfoFS.CloseScan())) {
        return rc;
    }
    if ((rc = rmManager->CloseFile(dbInfoFH))) {
        return rc;
    }

    // Open the system catalogs
    char relcatFileName[255] = "";
    char attrcatFileName[255] = "";
    if (distributed) {
        strcat(relcatFileName, "master/");
        strcat(attrcatFileName, "master/");
    }
    strcat(relcatFileName, "relcat");
    strcat(attrcatFileName, "attrcat");

    if ((rc = rmManager->OpenFile(relcatFileName, relcatFH))) {
        return rc;
    }
    if ((rc = rmManager->OpenFile(attrcatFileName, attrcatFH))) {
        return rc;
    }

    // Update flag
    isOpen = TRUE;

    // Return OK
    return OK_RC;
}


// Method: CloseDb()
// Close the database
/* Steps:
    1) Check that the database is not closed
    2) Close the system catalogs
    3) Update flag
*/
RC SM_Manager::CloseDb() {
    // Check if closed
    if (!isOpen) {
        return SM_DATABASE_CLOSED;
    }

    // Close the system catalogs
    int rc;
    if ((rc = rmManager->CloseFile(relcatFH))) {
        return rc;
    }
    if ((rc = rmManager->CloseFile(attrcatFH))) {
        return rc;
    }

    // Update flag
    isOpen = FALSE;

    // Return OK
    return OK_RC;
}


// TODO
// Method: CreateTable(const char *relName, int attrCount, AttrInfo *attributes)
// Create relation relName, given number of attributes and attribute data
/* Steps:
    1) Check that the database is open
    2) Check whether the table already exists
    3) Update the system catalogs
    4) Create a RM file for the relation
    5) Flush the system catalogs
*/
RC SM_Manager::CreateTable(const char *relName, int attrCount, AttrInfo *attributes) {
    // Check that the database is open
    if (!isOpen) {
        return SM_DATABASE_CLOSED;
    }

    // Check the parameters
    if (relName == NULL) {
        return SM_INVALID_NAME;
    }
    if (attrCount < 1 || attrCount > MAXATTRS) {
        return SM_INCORRECT_ATTRIBUTE_COUNT;
    }
    if (attributes == NULL) {
        return SM_NULL_ATTRIBUTES;
    }

    // Check whether the table already exists
    int rc;
    RM_FileScan relcatFS;
    RM_Record rec;
    bool duplicate = false;
    char relationName[MAXNAME+1];
    strcpy(relationName, relName);
    if ((rc = relcatFS.OpenScan(relcatFH, STRING, MAXNAME, 0, EQ_OP, relationName))) {
        return rc;
    }
    if ((rc = relcatFS.GetNextRec(rec)) != RM_EOF) {
        duplicate = true;
    }
    if ((rc = relcatFS.CloseScan())) {
        return rc;
    }
    if (duplicate) {
        return SM_TABLE_ALREADY_EXISTS;
    }

    // Print the create table command
    if (printCommands) {
        cout << "CreateTable\n"
             << "   relName     =" << relName << "\n"
             << "   attrCount   =" << attrCount << "\n";
        for (int i = 0; i < attrCount; i++) {
            cout << "   attributes[" << i << "].attrName=" << attributes[i].attrName
                 << "   attrType="
                 << (attributes[i].attrType == INT ? "INT" :
                     attributes[i].attrType == FLOAT ? "FLOAT" : "STRING")
                 << "   attrLength=" << attributes[i].attrLength << "\n";
        }
    }

    // Calculate the attribute information
    int tupleLength = 0;
    int offset[attrCount];
    for (int i=0; i<attrCount; i++) {
        offset[i] = tupleLength;
        tupleLength += attributes[i].attrLength;
    }

    // Update relcat
    RID rid;
    SM_RelcatRecord* rcRecord = new SM_RelcatRecord;
    memset(rcRecord, 0, sizeof(SM_RelcatRecord));
    strcpy(rcRecord->relName, relName);
    rcRecord->tupleLength = tupleLength;
    rcRecord->attrCount = attrCount;
    rcRecord->indexCount = 0;
    if ((rc = relcatFH.InsertRec((char*) rcRecord, rid))) {
        return rc;
    }
    delete rcRecord;

    // Update attrcat
    SM_AttrcatRecord* acRecord = new SM_AttrcatRecord;
    memset(acRecord, 0, sizeof(SM_AttrcatRecord));
    strcpy(acRecord->relName, relName);
    for (int i=0; i<attrCount; i++) {
        strcpy(acRecord->attrName, attributes[i].attrName);
        acRecord->offset = offset[i];
        acRecord->attrType = attributes[i].attrType;
        acRecord->attrLength = attributes[i].attrLength;
        acRecord->indexNo = -1;
        if ((rc = attrcatFH.InsertRec((char*) acRecord, rid))) {
            return rc;
        }
    }
    delete acRecord;

    // Create a RM file
    if ((rc = rmManager->CreateFile(relName, tupleLength))) {
        return rc;
    }

    // Flush the system catalogs
    if ((rc = relcatFH.ForcePages())) {
        return rc;
    }
    if ((rc = attrcatFH.ForcePages())) {
        return rc;
    }

    // Return OK
    return OK_RC;
}


// TODO
// Method: DropTable(const char *relName)
// Destroy a relation
/* Steps:
    1) Check that the database is open
    2) Print the drop table command
    3) Delete the entry from relcat
    4) Scan through attrcat
        - Destroy the indexes and delete the entries
    5) Destroy the RM file for the relation
    6) Flush the system catalogs
*/
RC SM_Manager::DropTable(const char *relName) {
    // Check that database is open
    if (!isOpen) {
        return SM_DATABASE_CLOSED;
    }

    // Check the parameters
    if (relName == NULL) {
        return SM_NULL_RELATION;
    }

    if (strcmp(relName, "relcat") == 0 || strcmp(relName, "attrcat") == 0) {
        return SM_SYSTEM_CATALOG;
    }

    // Print the drop table command
    if (printCommands) {
        cout << "DropTable\n   relName=" << relName << "\n";
    }

    // Delete the entry from relcat
    RM_FileScan relcatFS;
    RM_Record rec;
    int rc;

    // Get the record
    char relationName[MAXNAME+1];
    strcpy(relationName, relName);
    if ((rc = relcatFS.OpenScan(relcatFH, STRING, MAXNAME, 0, EQ_OP, relationName))) {
        return rc;
    }
    if ((rc = relcatFS.GetNextRec(rec))) {
        if (rc == RM_EOF) {
            return SM_TABLE_DOES_NOT_EXIST;
        }
        return rc;
    }

    // Get the RID of the record
    RID rid;
    if ((rc = rec.GetRid(rid))) {
        return rc;
    }

    // Delete the record
    if ((rc = relcatFH.DeleteRec(rid))) {
        return rc;
    }
    if ((rc = relcatFS.CloseScan())) {
        return rc;
    }

    // Scan through attrcat for the relation
    RM_FileScan attrcatFS;
    if ((rc = attrcatFS.OpenScan(attrcatFH, STRING, MAXNAME, 0, EQ_OP, relationName))) {
        return rc;
    }
    while ((rc = attrcatFS.GetNextRec(rec)) != RM_EOF) {
        if (rc) {
            return rc;
        }

        // Get the RID of the record
        if ((rc = rec.GetRid(rid))) {
            return rc;
        }

        // Check if index exists
        char* recordData;
        if ((rc = rec.GetData(recordData))) {
            return rc;
        }
        SM_AttrcatRecord* acRecord = (SM_AttrcatRecord*) recordData;
        if (acRecord->indexNo != -1) {
            // Destroy the index
            if ((rc = ixManager->DestroyIndex(relName, acRecord->indexNo))) {
                return rc;
            }
        }

        // Delete the record
        if ((rc = attrcatFH.DeleteRec(rid))) {
            return rc;
        }
    }

    // Flush the system catalogs
    if ((rc = relcatFH.ForcePages())) {
        return rc;
    }
    if ((rc = attrcatFH.ForcePages())) {
        return rc;
    }

    // Destroy the RM file for the relation
    if ((rc = rmManager->DestroyFile(relName))) {
        return rc;
    }

    // Return OK
    return OK_RC;
}


// TODO
// Method: CreateIndex(const char *relName, const char *attrName)
// Create an index for relName.attrName
/* Steps:
    1) Check the parameters
    2) Check that the database is open
    3) Check whether the index exists
    4) Update and flush the system catalogs
    5) Create and open the index file
    6) Scan all the tuples and insert in the index
    7) Close the index file
*/
RC SM_Manager::CreateIndex(const char *relName, const char *attrName) {
    // Check the parameters
    if (relName == NULL) {
        return SM_NULL_RELATION;
    }
    if (attrName == NULL) {
        return SM_NULL_ATTRIBUTES;
    }

    // Print the create index command
    if (printCommands) {
        cout << "CreateIndex\n"
             << "   relName =" << relName << "\n"
             << "   attrName=" << attrName << "\n";
    }

    // Check whether the index exists
    int rc;
    SM_AttrcatRecord* attrRecord = new SM_AttrcatRecord;
    memset(attrRecord, 0, sizeof(SM_AttrcatRecord));
    if ((rc = GetAttrInfo(relName, attrName, attrRecord))) {
        return rc;
    }
    if (attrRecord->indexNo != -1) {
        delete attrRecord;
        return SM_INDEX_EXISTS;
    }
    int offset = attrRecord->offset;
    AttrType attrType = attrRecord->attrType;
    int attrLength = attrRecord->attrLength;
    delete attrRecord;

    // Update relcat
    RM_FileScan relcatFS;
    RM_Record rec;
    char relationName[MAXNAME+1];
    strcpy(relationName, relName);
    if ((rc = relcatFS.OpenScan(relcatFH, STRING, MAXNAME, 0, EQ_OP, relationName))) {
        return rc;
    }
    if ((rc = relcatFS.GetNextRec(rec))) {
        if (rc == RM_EOF) {
            return SM_TABLE_DOES_NOT_EXIST;
        }
        else return rc;
    }
    char* recordData;
    if ((rc = rec.GetData(recordData))) {
        return rc;
    }
    SM_RelcatRecord* rcRecord = (SM_RelcatRecord*) recordData;
    rcRecord->indexCount++;
    if ((rc = relcatFH.UpdateRec(rec))) {
        return rc;
    }
    if ((rc = relcatFS.CloseScan())) {
        return rc;
    }

    // Update attrcat
    RM_FileScan attrcatFS;
    if ((rc = attrcatFS.OpenScan(attrcatFH, STRING, MAXNAME, 0, EQ_OP, relationName))) {
        return rc;
    }
    int position = 0;
    while (rc != RM_EOF) {
        rc = attrcatFS.GetNextRec(rec);
        if (rc != 0 && rc != RM_EOF) {
            return rc;
        }

        if (rc != RM_EOF) {
            if ((rc = rec.GetData(recordData))) {
                return rc;
            }
            SM_AttrcatRecord* acRecord = (SM_AttrcatRecord*) recordData;

            // Check for the required attribute
            if (strcmp(acRecord->attrName, attrName) == 0) {
                acRecord->indexNo = position;
                if ((rc = attrcatFH.UpdateRec(rec))) {
                    return rc;
                }
                break;
            }
        }
        position++;
    }
    if ((rc = attrcatFS.CloseScan())) {
        return rc;
    }

    // Flush the system catalogs
    if ((rc = relcatFH.ForcePages())) {
        return rc;
    }
    if ((rc = attrcatFH.ForcePages())) {
        return rc;
    }

    // Create and open the index file
    if ((rc = ixManager->CreateIndex(relName, position, attrType, attrLength))) {
        return rc;
    }
    IX_IndexHandle ixIH;
    if ((rc = ixManager->OpenIndex(relName, position, ixIH))) {
        return rc;
    }

    // Scan all the tuples in the relation
    RM_FileHandle rmFH;
    RM_FileScan rmFS;
    RID rid;
    if ((rc = rmManager->OpenFile(relName, rmFH))) {
        return rc;
    }
    if ((rc = rmFS.OpenScan(rmFH, INT, 4, 0, NO_OP, NULL))) {
        return rc;
    }
    while (rc != RM_EOF) {
        rc = rmFS.GetNextRec(rec);
        if (rc != 0 && rc != RM_EOF) {
            return rc;
        }

        // Get the record data and rid
        if (rc != RM_EOF) {
            if ((rc = rec.GetData(recordData))) {
                return rc;
            }
            if ((rc = rec.GetRid(rid))) {
                return rc;
            }

            // Insert the attribute value in the index
            if (attrType == INT) {
                int value;
                memcpy(&value, recordData+offset, sizeof(value));
                if ((rc = ixIH.InsertEntry(&value, rid))) {
                    return rc;
                }
            }
            else if (attrType == FLOAT) {
                float value;
                memcpy(&value, recordData+offset, sizeof(value));
                if ((rc = ixIH.InsertEntry(&value, rid))) {
                    return rc;
                }
            }
            else {
                char* value = new char[attrLength];
                strcpy(value, recordData+offset);
                if ((rc = ixIH.InsertEntry(value, rid))) {
                    return rc;
                }
                delete[] value;
            }
        }
    }
    if ((rc = rmFS.CloseScan())) {
        return rc;
    }

    // Close the files
    if ((rc = rmManager->CloseFile(rmFH))) {
        return rc;
    }
    if ((rc = ixManager->CloseIndex(ixIH))) {
        return rc;
    }

    // Return OK;
    return OK_RC;
}


// TODO
// Method: DropIndex(const char *relName, const char *attrName)
// Destroy index on relName.attrName
/* Steps:
    1) Check the parameters
    2) Check that the database is open
    3) Check whether the index exists
    4) Update and flush the system catalogs
    5) Destroy the index file
*/
RC SM_Manager::DropIndex(const char *relName, const char *attrName) {
    // Check the parameters
    if (relName == NULL) {
        return SM_NULL_RELATION;
    }
    if (attrName == NULL) {
        return SM_NULL_ATTRIBUTES;
    }

    // Print the drop index command
    if (printCommands) {
        cout << "DropIndex\n"
             << "   relName =" << relName << "\n"
             << "   attrName=" << attrName << "\n";
    }

    // Check whether the index exists
    int rc;
    SM_AttrcatRecord* attrRecord = new SM_AttrcatRecord;
    memset(attrRecord, 0, sizeof(SM_AttrcatRecord));
    if ((rc = GetAttrInfo(relName, attrName, attrRecord))) {
        return rc;
    }
    if (attrRecord->indexNo == -1) {
        delete attrRecord;
        return SM_INDEX_DOES_NOT_EXIST;
    }
    delete attrRecord;

    // Update relcat
    RM_FileScan relcatFS;
    RM_Record rec;
    char relationName[MAXNAME+1];
    strcpy(relationName, relName);
    if ((rc = relcatFS.OpenScan(relcatFH, STRING, MAXNAME, 0, EQ_OP, relationName))) {
        return rc;
    }
    if ((rc = relcatFS.GetNextRec(rec))) {
        if (rc == RM_EOF) {
            return SM_TABLE_DOES_NOT_EXIST;
        }
        else return rc;
    }
    char* recordData;
    if ((rc = rec.GetData(recordData))) {
        return rc;
    }
    SM_RelcatRecord* rcRecord = (SM_RelcatRecord*) recordData;
    rcRecord->indexCount--;
    if ((rc = relcatFH.UpdateRec(rec))) {
        return rc;
    }
    if ((rc = relcatFS.CloseScan())) {
        return rc;
    }

    // Update attrcat
    RM_FileScan attrcatFS;
    int position = -1;
    if ((rc = attrcatFS.OpenScan(attrcatFH, STRING, MAXNAME, 0, EQ_OP, relationName))) {
        return rc;
    }
    while (rc != RM_EOF) {
        rc = attrcatFS.GetNextRec(rec);
        if (rc != 0 && rc != RM_EOF) {
            return rc;
        }

        if (rc != RM_EOF) {
            if ((rc = rec.GetData(recordData))) {
                return rc;
            }
            SM_AttrcatRecord* acRecord = (SM_AttrcatRecord*) recordData;

            // Check for the required attribute
            if (strcmp(acRecord->attrName, attrName) == 0) {
                position = acRecord->indexNo;
                acRecord->indexNo = -1;
                if ((rc = attrcatFH.UpdateRec(rec))) {
                    return rc;
                }
                break;
            }
        }
    }
    if ((rc = attrcatFS.CloseScan())) {
        return rc;
    }

    // Flush the system catalogs
    if ((rc = relcatFH.ForcePages())) {
        return rc;
    }
    if ((rc = attrcatFH.ForcePages())) {
        return rc;
    }

    // Destroy the index file
    if ((rc = ixManager->DestroyIndex(relName, position))) {
        return rc;
    }

    // Return OK
    return OK_RC;
}


// TODO
// Method: Load(const char *relName, const char *fileName)
// Load relName from fileName
/* Steps:
    1) Check the parameters
    2) Check whether the database is open
    2) Obtain attribute information for the relation
    3) Open the RM file and each index file
    4) Open the data file
    5) Read the tuples from the file
        - Insert the tuple in the relation
        - Insert the entries in the indexes
    6) Close the files
*/
RC SM_Manager::Load(const char *relName, const char *fileName) {
    // Check the parameters
    if (relName == NULL) {
        return SM_NULL_RELATION;
    }
    if (fileName == NULL) {
        return SM_NULL_FILENAME;
    }

    if (strcmp(relName, "relcat") == 0 || strcmp(relName, "attrcat") == 0) {
        return SM_SYSTEM_CATALOG;
    }

    // Check that the database is open
    if (!isOpen) {
        return SM_DATABASE_CLOSED;
    }

    // Print the command
    if (printCommands) {
        cout << "Load\n"
             << "   relName =" << relName << "\n"
             << "   fileName=" << fileName << "\n";
    }

    // Get the relation and attributes information
    int rc;
    SM_RelcatRecord* rcRecord = new SM_RelcatRecord;
    memset(rcRecord, 0, sizeof(SM_RelcatRecord));
    if ((rc = GetRelInfo(relName, rcRecord))) {
        delete rcRecord;
        return rc;
    }
    int tupleLength = rcRecord->tupleLength;
    int attrCount = rcRecord->attrCount;
    int indexCount = rcRecord->indexCount;
    DataAttrInfo* attributes = new DataAttrInfo[attrCount];
    if ((rc = GetAttrInfo(relName, attrCount, (char*) attributes))) {
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

    // Open the data file
    ifstream dataFile(fileName);
    if (!dataFile.is_open()) {
        delete rcRecord;
        delete[] attributes;
        delete[] tupleData;
        delete[] ixIH;
        return SM_INVALID_DATA_FILE;
    }
    else {
        // Read each line of the file
        string line;
        while (getline(dataFile, line)) {
            // Parse the line
            stringstream ss(line);
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
        dataFile.close();
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

    // Clean up
    delete rcRecord;
    delete[] attributes;
    delete[] tupleData;
    delete[] ixIH;

    // Return OK
    return OK_RC;
}


// Method: Help()
// Print relations in db
/* Steps:
    1) Create the attributes structure
    2) Create a Printer object
    3) Print the header
    4) Start relcat file scan and print each tuple
    5) Print the footer
    6) Close the scan and clean up
*/
RC SM_Manager::Help() {
    // Check that the database is open
    if (!isOpen) {
        return SM_DATABASE_CLOSED;
    }

    // Print the command
    if (printCommands) {
        cout << "Help\n";
    }

    // Fill the attributes structure
    int attrCount = SM_RELCAT_ATTR_COUNT;
    RM_Record rec;
    int rc;
    char* recordData;

    DataAttrInfo* attributes = new DataAttrInfo[attrCount];
    if ((rc = GetAttrInfo("relcat", attrCount, (char*) attributes))) {
        return rc;
    }

    // Create a Printer object
    Printer p(attributes, attrCount);

    // Print the header
    p.PrintHeader(cout);

    // Start the relcat file scan
    RM_FileScan relcatFS;
    if ((rc = relcatFS.OpenScan(relcatFH, STRING, MAXNAME, 0, NO_OP, NULL))) {
        return rc;
    }

    // Print each tuple
    while (rc != RM_EOF) {
        rc = relcatFS.GetNextRec(rec);

        if (rc != 0 && rc != RM_EOF) {
            return rc;
        }

        if (rc != RM_EOF) {
            rec.GetData(recordData);
            p.Print(cout, recordData);
        }
    }

    // Print the footer
    p.PrintFooter(cout);

    // Close the scan and clean up
    if ((rc = relcatFS.CloseScan())) {
        return rc;
    }
    delete[] attributes;

    // Return OK
    return OK_RC;
}


// Method: Help(const char* relName)
// Print schema of relName
/* Steps:
    1) Create the attributes structure
    2) Create a Printer object
    3) Print the header
    4) Start attrcat scan and print each tuple
    5) Print the footer
    6) Close the scan and clean up
*/
RC SM_Manager::Help(const char *relName) {
    // Check that the database is open
    if (!isOpen) {
        return SM_DATABASE_CLOSED;
    }

    // Check the parameter
    if (relName == NULL) {
        return SM_NULL_RELATION;
    }

    // Print the command
    if (printCommands) {
        cout << "Help\n"
             << "   relName=" << relName << "\n";
    }

    // Check if the relation exists
    int rc;
    SM_RelcatRecord* rcRecord = new SM_RelcatRecord;
    memset(rcRecord, 0, sizeof(SM_RelcatRecord));
    if ((rc = GetRelInfo(relName, rcRecord))) {
        delete rcRecord;
        return rc;
    }
    delete rcRecord;

    // Fill the attributes structure
    int attrCount = SM_ATTRCAT_ATTR_COUNT;
    RM_Record rec;
    char* recordData;

    DataAttrInfo* attributes = new DataAttrInfo[attrCount];
    if ((rc = GetAttrInfo("attrcat", attrCount, (char*) attributes))) {
        return rc;
    }

    // Create a Printer object
    Printer p(attributes, attrCount);

    // Print the header
    p.PrintHeader(cout);

    // Start the attrcat file scan
    RM_FileScan attrcatFS;
    char relationName[MAXNAME+1];
    strcpy(relationName, relName);
    if ((rc = attrcatFS.OpenScan(attrcatFH, STRING, MAXNAME, 0, EQ_OP, relationName))) {
        return rc;
    }

    // Print each tuple
    while (rc != RM_EOF) {
        rc = attrcatFS.GetNextRec(rec);

        if (rc != 0 && rc != RM_EOF) {
            return rc;
        }

        if (rc != RM_EOF) {
            rec.GetData(recordData);
            p.Print(cout, recordData);
        }
    }

    // Print the footer
    p.PrintFooter(cout);

    // Close the scan and clean up
    if ((rc = attrcatFS.CloseScan())) {
        return rc;
    }
    delete[] attributes;

    // Return OK
    return OK_RC;
}


// TODO
// Method: Print(const char *relName)
// Print relName contents
/* Steps:
    1) Create the attributes structure
    2) Create a Printer object
    3) Print the header
    4) Open the RM file
    5) Start a RM file scan and print each tuple
    6) Print the footer
    7) Close the scan and file and clean up
*/
RC SM_Manager::Print(const char *relName) {
    // Check that the database is open
    if (!isOpen) {
        return SM_DATABASE_CLOSED;
    }

    // Check the parameters
    if (relName == NULL) {
        return SM_NULL_RELATION;
    }

    // Print the command
    if (printCommands) {
        cout << "Print\n"
             << "   relName=" << relName << "\n";
    }

    // Fill the attributes structure
    RM_Record rec;
    int rc;
    char* recordData;

    SM_RelcatRecord* rcRecord = new SM_RelcatRecord;
    memset(rcRecord, 0, sizeof(SM_RelcatRecord));
    if ((rc = GetRelInfo(relName, rcRecord))) {
        delete rcRecord;
        return rc;
    }

    int attrCount = rcRecord->attrCount;
    DataAttrInfo* attributes = new DataAttrInfo[attrCount];
    if ((rc = GetAttrInfo(relName, attrCount, (char*) attributes))) {
        return rc;
    }

    // Create a Printer object
    Printer p(attributes, attrCount);

    // Print the header
    p.PrintHeader(cout);

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

    // Print the footer
    p.PrintFooter(cout);

    // Close the scan and file and clean up
    if ((rc = rmFS.CloseScan())) {
        return rc;
    }
    if ((rc = rmManager->CloseFile(rmFH))) {
        return rc;
    }
    delete rcRecord;
    delete[] attributes;

    // Return OK
    return OK_RC;
}


// Method: Set(const char *paramName, const char *value)
// Set parameter to value
/* System parameters:
    1) printCommands - TRUE or FALSE
*/
RC SM_Manager::Set(const char *paramName, const char *value) {
    // Check the parameters
    if (paramName == NULL || value == NULL) {
        return SM_NULL_PARAMETERS;
    }

    // Set the system parameters
    if (strcmp(paramName, "printCommands") == 0) {
        if (strcmp(value, "TRUE") == 0) {
            printCommands = TRUE;
        }
        else if (strcmp(value, "FALSE") == 0) {
            printCommands = FALSE;
        }
        else {
            return SM_INVALID_VALUE;
        }
    }
    else if (strcmp(paramName, "optimizeQuery") == 0) {
        if (strcmp(value, "TRUE") == 0) {
            optimizeQuery = TRUE;
        }
        else if (strcmp(value, "FALSE") == 0) {
            optimizeQuery = FALSE;
        }
        else {
            return SM_INVALID_VALUE;
        }
    }
    else if (strcmp(paramName, "bQueryPlans") == 0) {
        if (strcmp(value, "1") == 0) {
            bQueryPlans = 1;
        }
        else if (strcmp(value, "0") == 0) {
            bQueryPlans = 0;
        }
        else {
            return SM_INVALID_VALUE;
        }
    }
     else {
        return SM_INVALID_SYSTEM_PARAMETER;
    }

    // Print the command
    if (printCommands) {
        cout << "Set\n"
             << "   paramName=" << paramName << "\n"
             << "   value    =" << value << "\n";
    }

    // Return OK
    return OK_RC;
}


// Method: GetAttrInfo(const char* relName, int attrCount, DataAttrInfo* attributes)
// Get the attribute info about a relation from attrcat
/* Steps:
    1) Start file scan of attrcat for relName
    2) For each record, fill attributes array
*/
RC SM_Manager::GetAttrInfo(const char* relName, int attrCount, char* attributeData) {
    // Check the parameters
    if (relName == NULL) {
        return SM_NULL_RELATION;
    }
    if (attrCount < 0) {
        return SM_INCORRECT_ATTRIBUTE_COUNT;
    }

    int rc;
    RM_FileScan attrcatFS;
    RM_Record rec;
    char* recordData;
    SM_AttrcatRecord* acRecord;
    DataAttrInfo* attributes = (DataAttrInfo*) attributeData;

    // Start file scan
    char relationName[MAXNAME+1];
    strcpy(relationName, relName);
    if ((rc = attrcatFS.OpenScan(attrcatFH, STRING, MAXNAME, 0, EQ_OP, relationName))) {
        return rc;
    }

    // Get all the attribute tuples
    int i = 0;
    while (rc != RM_EOF) {
        rc = attrcatFS.GetNextRec(rec);
        if (rc != 0 && rc != RM_EOF) {
            return rc;
        }

        if (rc != RM_EOF) {
            if (i == attrCount) {
                return SM_INCORRECT_ATTRIBUTE_COUNT;
            }

            if ((rc = rec.GetData(recordData))) {
                return rc;
            }
            acRecord = (SM_AttrcatRecord*) recordData;

            // Fill the attributes array
            strcpy(attributes[i].relName, acRecord->relName);
            strcpy(attributes[i].attrName, acRecord->attrName);
            attributes[i].offset = acRecord->offset;
            attributes[i].attrType = acRecord->attrType;
            attributes[i].attrLength = acRecord->attrLength;
            attributes[i].indexNo = acRecord->indexNo;
            i++;
        }
    }

    // Close the scan
    if ((rc = attrcatFS.CloseScan())) {
        return rc;
    }

    // Return OK
    return OK_RC;
}


// Method: GetAttrInfo(const char* relName, const char* attrName, SM_AttrcatRecord* attributeData)
// Get an attribute info about an attribute of a relation from attrcat
/* Steps:
    1) Start file scan of attrcat for relName
    2) For each record, check whether the required attribute
    3) Fill the attribute structure
*/
RC SM_Manager::GetAttrInfo(const char* relName, const char* attrName, SM_AttrcatRecord* attributeData) {
    // Check the parameters
    if (relName == NULL) {
        return SM_NULL_RELATION;
    }
    if (attrName == NULL) {
        return SM_NULL_ATTRIBUTES;
    }

    int rc;
    RM_FileScan attrcatFS;
    RM_Record rec;
    char* recordData;
    SM_AttrcatRecord* acRecord;

    // Start file scan
    char relationName[MAXNAME+1];
    strcpy(relationName, relName);
    if ((rc = attrcatFS.OpenScan(attrcatFH, STRING, MAXNAME, 0, EQ_OP, relationName))) {
        return rc;
    }

    // Get all the attribute tuples
    while (rc != RM_EOF) {
        rc = attrcatFS.GetNextRec(rec);
        if (rc != 0 && rc != RM_EOF) {
            return rc;
        }

        if (rc != RM_EOF) {
            if ((rc = rec.GetData(recordData))) {
                return rc;
            }
            acRecord = (SM_AttrcatRecord*) recordData;

            // Check for the required attribute
            if (strcmp(acRecord->attrName, attrName) == 0) {
                strcpy(attributeData->relName, acRecord->relName);
                strcpy(attributeData->attrName, acRecord->attrName);
                attributeData->offset = acRecord->offset;
                attributeData->attrType = acRecord->attrType;
                attributeData->attrLength = acRecord->attrLength;
                attributeData->indexNo = acRecord->indexNo;
                break;
            }
        }
    }

    if (rc == RM_EOF) {
        return SM_INVALID_ATTRIBUTE;
    }

    // Close the scan
    if ((rc = attrcatFS.CloseScan())) {
        return rc;
    }

    // Return OK
    return OK_RC;
}


// Method: GetRelInfo(const char* relName, SM_RelcatRecord* relationData)
// Get the relation info from relcat
/* Steps:
    1) Start file scan of relcat for relName
    2) Fill relation data
*/
RC SM_Manager::GetRelInfo(const char* relName, SM_RelcatRecord* relationData) {
    // Check the parameters
    if (relName == NULL) {
        return SM_NULL_RELATION;
    }

    int rc;
    RM_FileScan relcatFS;
    RM_Record rec;
    char* recordData;
    SM_RelcatRecord* rcRecord;

    // Start file scan
    char relationName[MAXNAME+1];
    strcpy(relationName, relName);
    if ((rc = relcatFS.OpenScan(relcatFH, STRING, MAXNAME, 0, EQ_OP, relationName))) {
        return rc;
    }

    // Get the relation tuple
    if ((rc = relcatFS.GetNextRec(rec))) {
        if (rc == RM_EOF) {
            return SM_TABLE_DOES_NOT_EXIST;
        }
    }
    if ((rc = rec.GetData(recordData))) {
        return rc;
    }
    rcRecord = (SM_RelcatRecord*) recordData;

    // Fill the relation data
    strcpy(relationData->relName, rcRecord->relName);
    relationData->tupleLength = rcRecord->tupleLength;
    relationData->attrCount = rcRecord->attrCount;
    relationData->indexCount = rcRecord->indexCount;

    // Close the scan
    if ((rc = relcatFS.CloseScan())) {
        return rc;
    }

    // Return OK
    return OK_RC;
}

// Method to get the printCommands flag
int SM_Manager::getPrintFlag() {
    return printCommands;
}

// Method to get the isOpen flag
int SM_Manager::getOpenFlag() {
    return isOpen;
}

// Method to get the distributed flag
int SM_Manager::getDistributedFlag() {
    return distributed;
}

// Method to get the number of nodes
int SM_Manager::getNumberNodes() {
    return numberNodes;
}

// Method to get the optimizeQuery flag
int SM_Manager::getOptimizeFlag() {
    return optimizeQuery;
}
