//
// dbcreate.cc
//
// Author: Jason McHugh (mchughj@cs.stanford.edu)
//
// This shell is provided for the student.
//
// Improved by: Aditya Bhandari (adityasb@stanford.edu)
//

#include <iostream>
#include <cstdio>
#include <string>
#include <unistd.h>
#include <sstream>
#include "rm.h"
#include "sm.h"
#include "redbase.h"

using namespace std;

//
// main
//
/* Steps:
    1) Create a subdirectory for the database
    2) EX - Create subdirectories for the master and data nodes
    3) EX - Store the database info in a file
    4) Create the system catalogs
        - Create RM files for relcat and attrcat
        - Open the files
        - Insert the relcat and attrcat records in relcat
        - Insert the attribute records in  attrcat
        - Close the files
*/
int main(int argc, char *argv[])
{
    char *dbname;
    char command[255] = "mkdir ";
    char commandCopy[255];
    int numberNodes = 1;
    RC rc;

    // Look for 2 or 4 arguments. The first is always the name of the program
    // that was executed, and the second should be the name of the database.
    // The third and fourth arguments are optional for the EX distributed case.
    if (argc != 2 && argc != 4) {
        cerr << "Usage: " << argv[0] << " dbname <optional -distributed numberNodes>\n";
        exit(1);
    }
    if (argc == 4) {
        // Check the second argument
        if (strcmp(argv[2], "-distributed") != 0) {
            cerr << "Invalid argument " << argv[2] << "\n";
            cerr << "Usage: " << argv[0] << " dbname <optional -distributed numberNodes>\n";
            exit(1);
        }

        // Check the number of nodes
        istringstream ss(argv[3]);
        if (!(ss >> numberNodes)) {
            cerr << "Invalid number of nodes " << argv[3] << "\n";
            cerr << "Usage: " << argv[0] << " dbname <optional -distributed numberNodes>\n";
            exit(1);
        }
        if (numberNodes <= 1) {
            cerr << "Please provide number of nodes greater than 1\n";
            cerr << "Usage: " << argv[0] << " dbname <optional -distributed numberNodes>\n";
            exit(1);
        }
    }

    // The database name is the second argument
    dbname = argv[1];

    // Create a subdirectory for the database
    strcpy(commandCopy, command);
    if (system(strcat(commandCopy,dbname)) != 0) {
        cerr << argv[0] << " cannot create directory: " << dbname << "\n";
        exit(1);
    }

    // Change to the database subdirectory
    if (chdir(dbname) < 0) {
        cerr << argv[0] << " chdir error to " << dbname << "\n";
        exit(1);
    }

    // EX - Create subdirectories for the master and data nodes, if specified
    if (numberNodes > 1) {
        strcpy(commandCopy, command);
        if (system(strcat(commandCopy,"master")) != 0) {
            cerr << argv[0] << " cannot create master node for the distributed database " << dbname << "\n";
            exit(1);
        }
        for (int i=1; i<=numberNodes; i++) {
            string dataNode = "data." + to_string(i);
            strcpy(commandCopy, command);
            if (system(strcat(commandCopy,dataNode.c_str())) != 0) {
                cerr << argv[0] << " cannot create data nodes for the distributed database " << dbname << "\n";
                exit(1);
            }
        }
    }

    PF_Manager pfManager;
    RM_Manager rmManager(pfManager);

    // EX - Store the database info in an RM file
    const char* dbInfoFileName = "dbinfo";
    if ((rc = rmManager.CreateFile(dbInfoFileName, sizeof(EX_DBInfo)))) {
        RM_PrintError(rc);
        return rc;
    }
    RM_FileHandle dbInfoFH;
    if ((rc = rmManager.OpenFile(dbInfoFileName, dbInfoFH))) {
        RM_PrintError(rc);
        return rc;
    }

    RID rid;
    EX_DBInfo* dbInfo = new EX_DBInfo;
    dbInfo->distributed = numberNodes > 1 ? TRUE : FALSE;
    dbInfo->numberNodes = numberNodes;
    if ((rc = dbInfoFH.InsertRec((char*) dbInfo, rid))) {
        RM_PrintError(rc);
        return rc;
    }
    if ((rc = rmManager.CloseFile(dbInfoFH))) {
        RM_PrintError(rc);
        return rc;
    }
    delete dbInfo;

    // EX - Change to the master subdirectory
    if (numberNodes > 1) {
        if (chdir("master") < 0) {
            cerr << argv[0] << " chdir error to master node directory\n";
            exit(1);
        }
    }

    // Create the system catalogs
    // Create RM files for relcat and attrcat
    const char* relcatFileName = "relcat";
    const char* attrcatFileName = "attrcat";
    if ((rc = rmManager.CreateFile(relcatFileName, sizeof(SM_RelcatRecord)))) {
        RM_PrintError(rc);
        return rc;
    }
    if ((rc = rmManager.CreateFile(attrcatFileName, sizeof(SM_AttrcatRecord)))) {
        RM_PrintError(rc);
        return rc;
    }

    // Open the files
    RM_FileHandle relcatFH;
    RM_FileHandle attrcatFH;
    if ((rc = rmManager.OpenFile(relcatFileName, relcatFH))) {
        RM_PrintError(rc);
        return rc;
    }
    if ((rc = rmManager.OpenFile(attrcatFileName, attrcatFH))) {
        RM_PrintError(rc);
        return rc;
    }

    // Insert relcat record in relcat
    SM_RelcatRecord* rcRecord = new SM_RelcatRecord;
    memset(rcRecord, 0, sizeof(SM_RelcatRecord));
    strcpy(rcRecord->relName, "relcat");
    rcRecord->tupleLength = sizeof(SM_RelcatRecord);
    rcRecord->attrCount = SM_RELCAT_ATTR_COUNT;
    rcRecord->indexCount = 0;
    // EX - distributed database
    rcRecord->distributed = FALSE;
    strcpy(rcRecord->attrName, "NA");
    rcRecord->attrType = (AttrType) 0;
    rcRecord->attrLength = 0;
    rcRecord->partitionVector = NULL;
    if ((rc = relcatFH.InsertRec((char*) rcRecord, rid))) {
        RM_PrintError(rc);
        return rc;
    }

    // Insert attrcat record in relcat
    strcpy(rcRecord->relName, "attrcat");
    rcRecord->tupleLength = sizeof(SM_AttrcatRecord);
    rcRecord->attrCount = SM_ATTRCAT_ATTR_COUNT;
    rcRecord->indexCount = 0;
    // EX - distributed database
    rcRecord->distributed = FALSE;
    strcpy(rcRecord->attrName, "NA");
    rcRecord->attrType = (AttrType) 0;
    rcRecord->attrLength = 0;
    rcRecord->partitionVector = NULL;
    if ((rc = relcatFH.InsertRec((char*) rcRecord, rid))) {
        RM_PrintError(rc);
        return rc;
    }
    delete rcRecord;

    // Insert relcat attributes in attrcat
    SM_AttrcatRecord* acRecord = new SM_AttrcatRecord;
    memset(acRecord, 0, sizeof(SM_AttrcatRecord));
    int currentOffset = 0;
    strcpy(acRecord->relName, "relcat");

    strcpy(acRecord->attrName, "relName");
    acRecord->offset = currentOffset;
    acRecord->attrType = STRING;
    acRecord->attrLength = MAXNAME+1;
    acRecord->indexNo = -1;
    if ((rc = attrcatFH.InsertRec((char*) acRecord, rid))) {
        RM_PrintError(rc);
        return rc;
    }

    currentOffset += MAXNAME+1;
    while (currentOffset % 4 != 0) currentOffset++;

    strcpy(acRecord->attrName, "tupleLength");
    acRecord->offset = currentOffset;
    acRecord->attrType = INT;
    acRecord->attrLength = 4;
    acRecord->indexNo = -1;
    if ((rc = attrcatFH.InsertRec((char*) acRecord, rid))) {
        RM_PrintError(rc);
        return rc;
    }

    currentOffset += 4;

    strcpy(acRecord->attrName, "attrCount");
    acRecord->offset = currentOffset;
    acRecord->attrType = INT;
    acRecord->attrLength = 4;
    acRecord->indexNo = -1;
    if ((rc = attrcatFH.InsertRec((char*) acRecord, rid))) {
        RM_PrintError(rc);
        return rc;
    }

    currentOffset += 4;

    strcpy(acRecord->attrName, "indexCount");
    acRecord->offset = currentOffset;
    acRecord->attrType = INT;
    acRecord->attrLength = 4;
    acRecord->indexNo = -1;
    if ((rc = attrcatFH.InsertRec((char*) acRecord, rid))) {
        RM_PrintError(rc);
        return rc;
    }

    currentOffset += 4;

    strcpy(acRecord->attrName, "distributed");
    acRecord->offset = currentOffset;
    acRecord->attrType = INT;
    acRecord->attrLength = 4;
    acRecord->indexNo = -1;
    if ((rc = attrcatFH.InsertRec((char*) acRecord, rid))) {
        RM_PrintError(rc);
        return rc;
    }

    currentOffset += 4;

    strcpy(acRecord->attrName, "attrName");
    acRecord->offset = currentOffset;
    acRecord->attrType = STRING;
    acRecord->attrLength = MAXNAME + 1;
    acRecord->indexNo = -1;
    if ((rc = attrcatFH.InsertRec((char*) acRecord, rid))) {
        RM_PrintError(rc);
        return rc;
    }

    currentOffset += MAXNAME+1;
    while (currentOffset % 4 != 0) currentOffset++;

    currentOffset += 4;

    strcpy(acRecord->attrName, "attrType");
    acRecord->offset = currentOffset;
    acRecord->attrType = INT;
    acRecord->attrLength = 4;
    acRecord->indexNo = -1;
    if ((rc = attrcatFH.InsertRec((char*) acRecord, rid))) {
        RM_PrintError(rc);
        return rc;
    }

    currentOffset += 4;

    strcpy(acRecord->attrName, "attrLength");
    acRecord->offset = currentOffset;
    acRecord->attrType = INT;
    acRecord->attrLength = 4;
    acRecord->indexNo = -1;
    if ((rc = attrcatFH.InsertRec((char*) acRecord, rid))) {
        RM_PrintError(rc);
        return rc;
    }

    currentOffset += 4;

    strcpy(acRecord->attrName, "partitionVector");
    acRecord->offset = currentOffset;
    acRecord->attrType = INT;
    acRecord->attrLength = 4;
    acRecord->indexNo = -1;
    if ((rc = attrcatFH.InsertRec((char*) acRecord, rid))) {
        RM_PrintError(rc);
        return rc;
    }

    // Insert attrcat attributes in attrcat
    currentOffset = 0;
    strcpy(acRecord->relName, "attrcat");

    strcpy(acRecord->attrName, "relName");
    acRecord->offset = currentOffset;
    acRecord->attrType = STRING;
    acRecord->attrLength = MAXNAME + 1;
    acRecord->indexNo = -1;
    if ((rc = attrcatFH.InsertRec((char*) acRecord, rid))) {
        RM_PrintError(rc);
        return rc;
    }

    currentOffset += MAXNAME+1;

    strcpy(acRecord->attrName, "attrName");
    acRecord->offset = currentOffset;
    acRecord->attrType = STRING;
    acRecord->attrLength = MAXNAME + 1;
    acRecord->indexNo = -1;
    if ((rc = attrcatFH.InsertRec((char*) acRecord, rid))) {
        RM_PrintError(rc);
        return rc;
    }

    currentOffset += MAXNAME+1;
    while (currentOffset % 4 != 0) currentOffset++;

    strcpy(acRecord->attrName, "offset");
    acRecord->offset = currentOffset;
    acRecord->attrType = INT;
    acRecord->attrLength = 4;
    acRecord->indexNo = -1;
    if ((rc = attrcatFH.InsertRec((char*) acRecord, rid))) {
        RM_PrintError(rc);
        return rc;
    }

    currentOffset += 4;

    strcpy(acRecord->attrName, "attrType");
    acRecord->offset = currentOffset;
    acRecord->attrType = INT;
    acRecord->attrLength = 4;
    acRecord->indexNo = -1;
    if ((rc = attrcatFH.InsertRec((char*) acRecord, rid))) {
        RM_PrintError(rc);
        return rc;
    }

    currentOffset += 4;

    strcpy(acRecord->attrName, "attrLength");
    acRecord->offset = currentOffset;
    acRecord->attrType = INT;
    acRecord->attrLength = 4;
    acRecord->indexNo = -1;
    if ((rc = attrcatFH.InsertRec((char*) acRecord, rid))) {
        RM_PrintError(rc);
        return rc;
    }

    currentOffset += 4;

    strcpy(acRecord->attrName, "indexNo");
    acRecord->offset = currentOffset;
    acRecord->attrType = INT;
    acRecord->attrLength = 4;
    acRecord->indexNo = -1;
    if ((rc = attrcatFH.InsertRec((char*) acRecord, rid))) {
        RM_PrintError(rc);
        return rc;
    }
    delete acRecord;

    // Close the files
    if ((rc = rmManager.CloseFile(relcatFH))) {
        RM_PrintError(rc);
        return rc;
    }
    if ((rc = rmManager.CloseFile(attrcatFH))) {
        RM_PrintError(rc);
        return rc;
    }

    return 0;
}
