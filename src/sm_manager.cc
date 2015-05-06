//
// File:        sm_manager.cc
// Description: SM_Manager class implementation
// Authors:     Aditya Bhandari (adityasb@stanford.edu)
//

#include <cstdio>
#include <unistd.h>
#include <iostream>
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
        return SM_DATABASE_DOES_NOT_EXIST;
    }

    // Open the system catalogs
    int rc;
    if ((rc = rmManager->OpenFile("relcat", relcatFH))) {
        return rc;
    }
    if ((rc = rmManager->OpenFile("attrcat", attrcatFH))) {
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
    3) Change to the up directory
    4) Update flag
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

    // Change to the up directory
    if (chdir("../") == -1) {
        return SM_INVALID_DATABASE_CLOSE;
    }

    // Update flag
    isOpen = FALSE;

    // Return OK
    return OK_RC;
}

// Method: CreateTable(const char *relName, int attrCount, AttrInfo *attributes)
// Create relation relName, given number of attributes and attribute data
/* Steps:
    1) Check that the database is open
    2) Print the create table command
    3) Update relcat
    4) Update attrcat
    5) Create a RM file for the relation
    6) Flush the system catalogs
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

    // Print the create table command
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

    // Calculate the attribute information
    int tupleLength = 0;
    int offset[attrCount];
    for (int i=0; i<attrCount; i++) {
        offset[i] = tupleLength;
        tupleLength += attributes[i].attrLength;
    }

    // Update relcat
    int rc;
    RID rid;
    SM_RelcatRecord* rcRecord = new SM_RelcatRecord;
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
    strcpy(acRecord->relName, relName);
    for (int i=0; i<attrCount; i++) {
        memset(acRecord->attrName, 0, MAXNAME+1);
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
        return SM_INVALID_NAME;
    }

    // Print the drop table command
    cout << "DropTable\n   relName=" << relName << "\n";

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
            char indexName[2*MAXNAME];
            strcpy(indexName, relName);
            strcat(indexName, ".");
            strcat(indexName, acRecord->attrName);
            if ((rc = ixManager->DestroyIndex(indexName, acRecord->indexNo))) {
                return rc;
            }
        }

        // Delete the record
        if ((rc = attrcatFH.DeleteRec(rid))) {
            return rc;
        }
    }

    // Destroy the RM file for the relation
    if ((rc = rmManager->DestroyFile(relName))) {
        return rc;
    }

    // Return OK
    return OK_RC;
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

// Method: Help()
// Print relations in db
/* Steps:
    1) Create the attributes structure
    2) Create a Printer object
    3) Print the header
    4) Start a RM file scan and print each tuple
    5) Print the footer
    6) Close the scan and clean up
*/
RC SM_Manager::Help() {
    // Print the command
    cout << "Help\n";

    // Fill the attributes structure
    int attrCount = 4;
    RM_Record rec;
    int rc;
    char* recordData;

    DataAttrInfo* attributes = new DataAttrInfo[attrCount];
    if ((rc = GetAttrInfo("relcat", 4, (char*) attributes))) {
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
    4) Start a RM file scan and print each tuple
    5) Print the footer
    6) Close the scan and clean up
*/
RC SM_Manager::Help(const char *relName) {
    // Print the command
    cout << "Help\n"
         << "   relName=" << relName << "\n";

    //TODO
    // Fill the attributes structure
    RM_Record rec;
    int rc;
    char* recordData;

    SM_RelcatRecord* rcRecord = new SM_RelcatRecord;
    if ((rc = GetRelInfo(relName, rcRecord))) {
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
        return rc;
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

// Method: Print(const char *relName)
// Print relName contents
/* Steps:
    1) Create the attributes structure
    2) Create a Printer object
    3) Print the header
    4) Start a RM file scan and print each tuple
    5) Print the footer
    6) Close the scan and file and clean up
*/
RC SM_Manager::Print(const char *relName) {
    cout << "Print\n"
         << "   relName=" << relName << "\n";
    return (0);

    // Fill the attributes structure
    RM_Record rec;
    int rc;
    char* recordData;

    SM_RelcatRecord* rcRecord = new SM_RelcatRecord;
    if ((rc = GetRelInfo(relName, rcRecord))) {
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
        return rc;
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

// Set parameter to value
RC SM_Manager::Set(const char *paramName, const char *value) {
    cout << "Set\n"
         << "   paramName=" << paramName << "\n"
         << "   value    =" << value << "\n";
    return (0);
}

// Method: GetAttrInfo(const char* relName, int attrCount, DataAttrInfo* attributes)
// Get the attribute info about a relation from attrcat
/* Steps:
    1) Start file scan of attrcat for relName
    2) For each record, fill attributes array
*/
RC SM_Manager::GetAttrInfo(const char* relName, int attrCount, char* attributeData) {
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

// Method: GetRelInfo(const char* relName, SM_RelcatRecord* relationData)
// Get the relation info from relcat
/* Steps:
    1) Start file scan of relcat for relName
    2) Fill relation data
*/
RC SM_Manager::GetRelInfo(const char* relName, SM_RelcatRecord* relationData) {
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
