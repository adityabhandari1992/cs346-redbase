//
// dbcreate.cc
//
// Author: Jason McHugh (mchughj@cs.stanford.edu)
//
// This shell is provided for the student.
//
// Improved by: Aditya Bhandari (adityasb@stanford.edu)

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
    if ((rc = rmManager.CreateFile(relcatFileName, MAXNAME + 1 + 12))) {
        RM_PrintError(rc);
    }
    if ((rc = rmManager.CreateFile(attrcatFileName, 2*(MAXNAME+1) + 16))) {
        RM_PrintError(rc);
    }

    // Open the files
    RM_FileHandle relcatFH;
    RM_FileHandle attrcatFH;
    if ((rc = rmManager.OpenFile(relcatFileName, relcatFH))) {
        RM_PrintError(rc);
    }
    if ((rc = rmManager.OpenFile(attrcatFileName, attrcatFH))) {
        RM_PrintError(rc);
    }

    // Insert relcat record in relcat
    RID rid;
    SM_RelcatRecord* rcRecord = new SM_RelcatRecord;
    strcpy(rcRecord->relName, "relcat");
    rcRecord->tupleLength = MAXNAME + 1 + 12;
    rcRecord->attrCount = 4;
    rcRecord->indexCount = 0;
    if ((rc = relcatFH.InsertRec((char*) rcRecord, rid))) {
        RM_PrintError(rc);
    }

    // Insert attrcat record in relcat
    strcpy(rcRecord->relName, "attrcat");
    rcRecord->tupleLength = 2*(MAXNAME+1) + 16;
    rcRecord->attrCount = 6;
    rcRecord->indexCount = 0;
    if ((rc = relcatFH.InsertRec((char*) rcRecord, rid))) {
        RM_PrintError(rc);
    }
    delete rcRecord;

    // Insert relcat attributes in attrcat
    SM_AttrcatRecord* acRecord = new SM_AttrcatRecord;
    strcpy(acRecord->relName, "relcat");

    strcpy(acRecord->attrName, "relName");
    acRecord->offset = 0;
    acRecord->attrType = STRING;
    acRecord->attrLength = MAXNAME+1;
    acRecord->indexNo = -1;
    if ((rc = attrcatFH.InsertRec((char*) acRecord, rid))) {
        RM_PrintError(rc);
    }

    strcpy(acRecord->attrName, "tupleLength");
    acRecord->offset = 28;
    acRecord->attrType = INT;
    acRecord->attrLength = 4;
    acRecord->indexNo = -1;
    if ((rc = attrcatFH.InsertRec((char*) acRecord, rid))) {
        RM_PrintError(rc);
    }

    strcpy(acRecord->attrName, "attrCount");
    acRecord->offset = 32;
    acRecord->attrType = INT;
    acRecord->attrLength = 4;
    acRecord->indexNo = -1;
    if ((rc = attrcatFH.InsertRec((char*) acRecord, rid))) {
        RM_PrintError(rc);
    }

    strcpy(acRecord->attrName, "indexCount");
    acRecord->offset = 36;
    acRecord->attrType = INT;
    acRecord->attrLength = 4;
    acRecord->indexNo = -1;
    if ((rc = attrcatFH.InsertRec((char*) acRecord, rid))) {
        RM_PrintError(rc);
    }

    // Insert relcat attributes in attrcat
    strcpy(acRecord->relName, "attrcat");

    strcpy(acRecord->attrName, "relName");
    acRecord->offset = 0;
    acRecord->attrType = STRING;
    acRecord->attrLength = MAXNAME + 1;
    acRecord->indexNo = -1;
    if ((rc = attrcatFH.InsertRec((char*) acRecord, rid))) {
        RM_PrintError(rc);
    }

    strcpy(acRecord->attrName, "attrName");
    acRecord->offset = 25;
    acRecord->attrType = STRING;
    acRecord->attrLength = MAXNAME + 1;
    acRecord->indexNo = -1;
    if ((rc = attrcatFH.InsertRec((char*) acRecord, rid))) {
        RM_PrintError(rc);
    }

    strcpy(acRecord->attrName, "offset");
    acRecord->offset = 52;
    acRecord->attrType = INT;
    acRecord->attrLength = 4;
    acRecord->indexNo = -1;
    if ((rc = attrcatFH.InsertRec((char*) acRecord, rid))) {
        RM_PrintError(rc);
    }

    strcpy(acRecord->attrName, "attrType");
    acRecord->offset = 56;
    acRecord->attrType = INT;
    acRecord->attrLength = 4;
    acRecord->indexNo = -1;
    if ((rc = attrcatFH.InsertRec((char*) acRecord, rid))) {
        RM_PrintError(rc);
    }

    strcpy(acRecord->attrName, "attrLength");
    acRecord->offset = 60;
    acRecord->attrType = INT;
    acRecord->attrLength = 4;
    acRecord->indexNo = -1;
    if ((rc = attrcatFH.InsertRec((char*) acRecord, rid))) {
        RM_PrintError(rc);
    }

    strcpy(acRecord->attrName, "indexNo");
    acRecord->offset = 64;
    acRecord->attrType = INT;
    acRecord->attrLength = 4;
    acRecord->indexNo = -1;
    if ((rc = attrcatFH.InsertRec((char*) acRecord, rid))) {
        RM_PrintError(rc);
    }
    delete acRecord;

    // Close the files
    if ((rc = rmManager.CloseFile(relcatFH))) {
        RM_PrintError(rc);
    }
    if ((rc = rmManager.CloseFile(attrcatFH))) {
        RM_PrintError(rc);
    }

    return(0);
}
