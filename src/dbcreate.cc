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
    if ((rc = rmManager.CreateFile(relcatFileName, sizeof(SM_RelcatRecord)))) {
        RM_PrintError(rc);
    }
    if ((rc = rmManager.CreateFile(attrcatFileName, sizeof(SM_AttrcatRecord)))) {
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
    memset(rcRecord->relName, ' ', MAXNAME);
    sprintf(rcRecord->relName, "relcat");
    rcRecord->tupleLength = sizeof(SM_RelcatRecord);
    rcRecord->attrCount = 4;
    rcRecord->indexCount = 0;
    if ((rc = relcatFH.InsertRec((char*) rcRecord, rid))) {
        RM_PrintError(rc);
    }

    // Insert attrcat record in relcat
    memset(rcRecord->relName, ' ', MAXNAME);
    sprintf(rcRecord->relName, "attrcat");
    rcRecord->tupleLength = sizeof(SM_AttrcatRecord);
    rcRecord->attrCount = 6;
    rcRecord->indexCount = 0;
    if ((rc = relcatFH.InsertRec((char*) rcRecord, rid))) {
        RM_PrintError(rc);
    }
    delete rcRecord;

    // Insert relcat attributes in attrcat
    SM_AttrcatRecord* acRecord = new SM_AttrcatRecord;
    memset(acRecord->relName, ' ', MAXNAME);
    sprintf(acRecord->relName, "relcat");

    memset(acRecord->attrName, ' ', MAXNAME);
    sprintf(acRecord->attrName, "relName");
    acRecord->offset = 0;
    acRecord->attrType = STRING;
    acRecord->attrLength = MAXNAME;
    acRecord->indexNo = -1;
    if ((rc = attrcatFH.InsertRec((char*) acRecord, rid))) {
        RM_PrintError(rc);
    }

    memset(acRecord->attrName, ' ', MAXNAME);
    sprintf(acRecord->attrName, "tupleLength");
    acRecord->offset = MAXNAME;
    acRecord->attrType = INT;
    acRecord->attrLength = 4;
    acRecord->indexNo = -1;
    if ((rc = attrcatFH.InsertRec((char*) acRecord, rid))) {
        RM_PrintError(rc);
    }

    memset(acRecord->attrName, ' ', MAXNAME);
    sprintf(acRecord->attrName, "attrCount");
    acRecord->offset = MAXNAME + 4;
    acRecord->attrType = INT;
    acRecord->attrLength = 4;
    acRecord->indexNo = -1;
    if ((rc = attrcatFH.InsertRec((char*) acRecord, rid))) {
        RM_PrintError(rc);
    }

    memset(acRecord->attrName, ' ', MAXNAME);
    sprintf(acRecord->attrName, "indexCount");
    acRecord->offset = MAXNAME + 8;
    acRecord->attrType = INT;
    acRecord->attrLength = 4;
    acRecord->indexNo = -1;
    if ((rc = attrcatFH.InsertRec((char*) acRecord, rid))) {
        RM_PrintError(rc);
    }

    // Insert relcat attributes in attrcat
    memset(acRecord->relName, ' ', MAXNAME);
    sprintf(acRecord->relName, "attrcat");

    memset(acRecord->attrName, ' ', MAXNAME);
    sprintf(acRecord->attrName, "relName");
    acRecord->offset = 0;
    acRecord->attrType = STRING;
    acRecord->attrLength = MAXNAME;
    acRecord->indexNo = -1;
    if ((rc = attrcatFH.InsertRec((char*) acRecord, rid))) {
        RM_PrintError(rc);
    }

    memset(acRecord->attrName, ' ', MAXNAME);
    sprintf(acRecord->attrName, "attrName");
    acRecord->offset = MAXNAME;
    acRecord->attrType = STRING;
    acRecord->attrLength = MAXNAME;
    acRecord->indexNo = -1;
    if ((rc = attrcatFH.InsertRec((char*) acRecord, rid))) {
        RM_PrintError(rc);
    }

    memset(acRecord->attrName, ' ', MAXNAME);
    sprintf(acRecord->attrName, "offset");
    acRecord->offset = 2*MAXNAME;
    acRecord->attrType = INT;
    acRecord->attrLength = 4;
    acRecord->indexNo = -1;
    if ((rc = attrcatFH.InsertRec((char*) acRecord, rid))) {
        RM_PrintError(rc);
    }

    memset(acRecord->attrName, ' ', MAXNAME);
    sprintf(acRecord->attrName, "attrType");
    acRecord->offset = 2*MAXNAME + 4;
    acRecord->attrType = INT;
    acRecord->attrLength = 4;
    acRecord->indexNo = -1;
    if ((rc = attrcatFH.InsertRec((char*) acRecord, rid))) {
        RM_PrintError(rc);
    }

    memset(acRecord->attrName, ' ', MAXNAME);
    sprintf(acRecord->attrName, "attrLength");
    acRecord->offset = 2*MAXNAME + 8;
    acRecord->attrType = INT;
    acRecord->attrLength = 4;
    acRecord->indexNo = -1;
    if ((rc = attrcatFH.InsertRec((char*) acRecord, rid))) {
        RM_PrintError(rc);
    }

    memset(acRecord->attrName, ' ', MAXNAME);
    sprintf(acRecord->attrName, "indexNo");
    acRecord->offset = 2*MAXNAME + 12;
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
