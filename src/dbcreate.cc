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
#include <cstring>
#include <unistd.h>
#include "rm.h"
#include "sm.h"
#include "redbase.h"

using namespace std;

//
// main
//
/* Steps:
    1) Create a subdirectory for the database
    2) Create the system catalogs
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
    RC rc;

    // Look for 2 arguments. The first is always the name of the program
    // that was executed, and the second should be the name of the
    // database.
    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " dbname \n";
        exit(1);
    }

    // The database name is the second argument
    dbname = argv[1];

    // Create a subdirectory for the database
    if (system (strcat(command,dbname)) != 0) {
        cerr << argv[0] << " cannot create directory: " << dbname << "\n";
        exit(1);
    }

    if (chdir(dbname) < 0) {
        cerr << argv[0] << " chdir error to " << dbname << "\n";
        exit(1);
    }

    // Create the system catalogs
    PF_Manager pfManager;
    RM_Manager rmManager(pfManager);

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
    RID rid;
    SM_RelcatRecord* rcRecord = new SM_RelcatRecord;
    memset(rcRecord, 0, sizeof(SM_RelcatRecord));
    strcpy(rcRecord->relName, "relcat");
    rcRecord->tupleLength = sizeof(SM_RelcatRecord);
    rcRecord->attrCount = SM_RELCAT_ATTR_COUNT;
    rcRecord->indexCount = 0;
    if ((rc = relcatFH.InsertRec((char*) rcRecord, rid))) {
        RM_PrintError(rc);
        return rc;
    }

    // Insert attrcat record in relcat
    strcpy(rcRecord->relName, "attrcat");
    rcRecord->tupleLength = sizeof(SM_AttrcatRecord);
    rcRecord->attrCount = SM_ATTRCAT_ATTR_COUNT;
    rcRecord->indexCount = 0;
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

    // Insert relcat attributes in attrcat
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
