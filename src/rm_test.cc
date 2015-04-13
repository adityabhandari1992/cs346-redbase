//
// File:        rm_testshell.cc
// Description: Test RM component
// Authors:     Jan Jannink
//              Dallan Quass (quass@cs.stanford.edu)
//              Jason McHugh (mchughj@cs.stanford.edu)
//
// This test shell contains a number of functions that will be useful
// in testing your RM component code.  In addition, a couple of sample
// tests are provided.  The tests are by no means comprehensive, however,
// and you are expected to devise your own tests to test your code.
//
// 1997:  Tester has been modified to reflect the change in the 1997
// interface.  For example, FileHandle no longer supports a Scan over the
// relation.  All scans are done via a FileScan.
//

#include <cstdio>
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <cstdlib>

#include "redbase.h"
#include "pf.h"
#include "rm.h"

using namespace std;

//
// Defines
//
#define FILENAME   "testrel"         // test file name
#define STRLEN      29               // length of string in testrec
#define PROG_UNIT   50               // how frequently to give progress
                                      //   reports when adding lots of recs
#define FEW_RECS   1200                // number of records added in

//
// Computes the offset of a field in a record (should be in <stddef.h>)
//
#ifndef offsetof
#       define offsetof(type, field)   ((size_t)&(((type *)0) -> field))
#endif

#ifdef PF_STATS
#include "statistics.h"

// This is defined within pf_buffermgr.cc
extern StatisticsMgr *pStatisticsMgr;

// This method is defined within pf_statistics.cc.  It is called at the end
// to display the final statistics, or by the debugger to monitor progress.
extern void PF_Statistics();

//
// PF_ConfirmStatistics
//
// This function will be run at the end of the program after all the tests
// to confirm that the buffer manager operated correctly.
//
// These numbers have been confirmed.  Note that if you change any of the
// tests, you will also need to change these numbers as well.
//
void PF_ConfirmStatistics()
{
   // Must remember to delete the memory returned from StatisticsMgr::Get
   cout << "Verifying the statistics for buffer manager: ";
   int *piGP = pStatisticsMgr->Get("GetPage");
   int *piPF = pStatisticsMgr->Get("PageFound");
   int *piPNF = pStatisticsMgr->Get("PageNotFound");
   int *piWP = pStatisticsMgr->Get("WritePage");
   int *piRP = pStatisticsMgr->Get("ReadPage");
   int *piFP = pStatisticsMgr->Get("FlushPage");

   if (piGP && (*piGP != 702)) {
      cout << "Number of GetPages is incorrect! (" << *piGP << ")\n";
      // No built in error code for this
      exit(1);
   }
   if (piPF && (*piPF != 23)) {
      cout << "Number of pages found in the buffer is incorrect! (" <<
        *piPF << ")\n";
      // No built in error code for this
      exit(1);
   }
   if (piPNF && (*piPNF != 679)) {
      cout << "Number of pages not found in the buffer is incorrect! (" <<
        *piPNF << ")\n";
      // No built in error code for this
      exit(1);
   }
   if (piRP && (*piRP != 679)) {
      cout << "Number of read requests to the Unix file system is " <<
         "incorrect! (" << *piPNF << ")\n";
      // No built in error code for this
      exit(1);
   }
   if (piWP && (*piWP != 339)) {
      cout << "Number of write requests to the Unix file system is "<<
         "incorrect! (" << *piPNF << ")\n";
      // No built in error code for this
      exit(1);
   }
   if (piFP && (*piFP != 16)) {
      cout << "Number of requests to flush the buffer is "<<
         "incorrect! (" << *piPNF << ")\n";
      // No built in error code for this
      exit(1);
   }
   cout << " Correct!\n";

   // Delete the memory returned from StatisticsMgr::Get
   delete piGP;
   delete piPF;
   delete piPNF;
   delete piWP;
   delete piRP;
   delete piFP;
}
#endif    // PF_STATS


//
// Structure of the records we will be using for the tests
//
struct TestRec {
    char  str[STRLEN];
    int   num;
    float r;
};

//
// Global PF_Manager and RM_Manager variables
//
PF_Manager pfm;
RM_Manager rmm(pfm);

//
// Function declarations
//
RC Test1(void);
RC Test2(void);
RC Test3(void);
RC Test4(void);
RC Test5(void);

void PrintError(RC rc);
void LsFile(char *fileName);
void PrintRecord(TestRec &recBuf);
RC AddRecs(RM_FileHandle &fh, int numRecs);
RC VerifyFile(RM_FileHandle &fh, int numRecs, CompOp op);
RC PrintFile(RM_FileHandle &fh);

RC CreateFile(char *fileName, int recordSize);
RC DestroyFile(char *fileName);
RC OpenFile(char *fileName, RM_FileHandle &fh);
RC CloseFile(char *fileName, RM_FileHandle &fh);
RC GetRec(RM_FileHandle &fh, RID &rid, RM_Record &rec);
RC InsertRec(RM_FileHandle &fh, char *record, RID &rid);
RC UpdateRec(RM_FileHandle &fh, RM_Record &rec);
RC DeleteRec(RM_FileHandle &fh, RID &rid);
RC GetNextRecScan(RM_FileScan &fs, RM_Record &rec);

//
// Array of pointers to the test functions
//
#define NUM_TESTS       5               // number of tests
int (*tests[])() =                      // RC doesn't work on some compilers
{
    Test1,
    Test2,
    Test3,
    Test4,
    Test5
};

//
// main
//
int main(int argc, char *argv[])
{
    RC   rc;
    char *progName = argv[0];   // since we will be changing argv
    int  testNum;

    // Write out initial starting message
    cerr.flush();
    cout.flush();
    cout << "Starting RM component test.\n";
    cout.flush();

    // Delete files from last time
    unlink(FILENAME);

    // If no argument given, do all tests
    if (argc == 1) {
        for (testNum = 0; testNum < NUM_TESTS; testNum++)
            if ((rc = (tests[testNum])())) {

                // Print the error and exit
                PrintError(rc);
                return (1);
            }
    }
    else {

        // Otherwise, perform specific tests
        while (*++argv != NULL) {

            // Make sure it's a number
            if (sscanf(*argv, "%d", &testNum) != 1) {
                cerr << progName << ": " << *argv << " is not a number\n";
                continue;
            }

            // Make sure it's in range
            if (testNum < 1 || testNum > NUM_TESTS) {
                cerr << "Valid test numbers are between 1 and " << NUM_TESTS << "\n";
                continue;
            }

            // Perform the test
            if ((rc = (tests[testNum - 1])())) {

                // Print the error and exit
                PrintError(rc);
                return (1);
            }
        }
    }

    // Write ending message and exit
    cout << "Ending RM component test.\n\n";

    return (0);
}

//
// PrintError
//
// Desc: Print an error message by calling the proper component-specific
//       print-error function
//
void PrintError(RC rc)
{
    if (abs(rc) <= END_PF_WARN)
        PF_PrintError(rc);
    else if (abs(rc) <= END_RM_WARN)
        RM_PrintError(rc);
    else
        cerr << "Error code out of range: " << rc << "\n";
}

////////////////////////////////////////////////////////////////////
// The following functions may be useful in tests that you devise //
////////////////////////////////////////////////////////////////////

//
// LsFile
//
// Desc: list the filename's directory entry
//
void LsFile(char *fileName)
{
    char command[80];

    sprintf(command, "ls -l %s", fileName);
    printf("doing \"%s\"\n", command);
    system(command);
}

//
// PrintRecord
//
// Desc: Print the TestRec record components
//
void PrintRecord(TestRec &recBuf)
{
    printf("[%s, %d, %f]\n", recBuf.str, recBuf.num, recBuf.r);
}

//
// AddRecs
//
// Desc: Add a number of records to the file
//
RC AddRecs(RM_FileHandle &fh, int numRecs)
{
    RC      rc;
    int     i;
    TestRec recBuf;
    RID     rid;
    PageNum pageNum;
    SlotNum slotNum;

    // We set all of the TestRec to be 0 initially.  This heads off
    // warnings that Purify will give regarding UMR since sizeof(TestRec)
    // is 40, whereas actual size is 37.
    memset((void *)&recBuf, 0, sizeof(recBuf));

    printf("\nadding %d records\n", numRecs);
    for (i = 0; i < numRecs; i++) {
        memset(recBuf.str, ' ', STRLEN);
        sprintf(recBuf.str, "a%d", i);
        recBuf.num = i;
        recBuf.r = (float)i;
        if ((rc = InsertRec(fh, (char *)&recBuf, rid)) ||
            (rc = rid.GetPageNum(pageNum)) ||
            (rc = rid.GetSlotNum(slotNum)))
            return (rc);

        if ((i + 1) % PROG_UNIT == 0){
            printf("%d  ", i + 1);
            fflush(stdout);
        }
    }
    if (i % PROG_UNIT != 0)
        printf("%d\n", i);
    else
        putchar('\n');

    // Return ok
    return (0);
}

//
// VerifyFile
//
// Desc: verify that a file has records as added by AddRecs
//
RC VerifyFile(RM_FileHandle &fh, int numRecs, CompOp op)
{
    RC        rc;
    int       n;
    TestRec   *pRecBuf;
    RID       rid;
    char      stringBuf[STRLEN];
    char      *found;
    RM_Record rec;

    printf("\nverifying file contents\n");

    RM_FileScan fs;
    TestRec* tempRecord = new TestRec();
    tempRecord->num = 100;
    if ((rc=fs.OpenScan(fh,INT,sizeof(int),offsetof(TestRec, num),
                        op, (void*) tempRecord + offsetof(TestRec, num), NO_HINT)))
        return (rc);

    found = new char[numRecs];
    memset(found, 0, numRecs);

    int givenValue = *static_cast<int*>((void*)tempRecord + offsetof(TestRec, num));

    // For each record in the file
    for (rc = GetNextRecScan(fs, rec), n = 0;
         rc == 0;
         rc = GetNextRecScan(fs, rec), n++) {

        // Make sure the record is correct
        if ((rc = rec.GetData((char *&)pRecBuf)) ||
            (rc = rec.GetRid(rid)))
            goto err;

        memset(stringBuf,' ', STRLEN);
        sprintf(stringBuf, "a%d", pRecBuf->num);

        // if (pRecBuf->num < 0 || pRecBuf->num >= numRecs ||
        //     strcmp(pRecBuf->str, stringBuf) ||
        //     pRecBuf->r != (float)pRecBuf->num) {
        //     printf("VerifyFile: invalid record = [%s, %d, %f]\n",
        //            pRecBuf->str, pRecBuf->num, pRecBuf->r);
        //     exit(1);
        // }

        if (found[pRecBuf->num]) {
            printf("VerifyFile: duplicate record = [%s, %d, %f]\n",
                   pRecBuf->str, pRecBuf->num, pRecBuf->r);
            exit(1);
        }

        found[pRecBuf->num] = 1;
    }

    if (rc != RM_EOF)
        goto err;

    delete tempRecord;
    delete[] found;

    if ((rc=fs.CloseScan()))
        return (rc);

    // make sure we had the right number of records in the file
    if (n != numRecs) {
        printf("%d records in file (supposed to be %d)\n",
               n, numRecs);
        // exit(1);
    }
    else {
        printf("Success!\n");
    }

    // Return ok
    rc = 0;

    return rc;

err:
    fs.CloseScan();
    delete[] found;
    return (rc);
}

//
// PrintFile
//
// Desc: Print the contents of the file
//
RC PrintFile(RM_FileScan &fs)
{
    RC        rc;
    int       n;
    TestRec   *pRecBuf;
    RID       rid;
    RM_Record rec;

    printf("\nprinting file contents\n");

    // for each record in the file
    for (rc = GetNextRecScan(fs, rec), n = 0;
         rc == 0;
         rc = GetNextRecScan(fs, rec), n++) {

        // Get the record data and record id
        if ((rc = rec.GetData((char *&)pRecBuf)) ||
            (rc = rec.GetRid(rid)))
            return (rc);

        // Print the record contents
        PrintRecord(*pRecBuf);
    }

    if (rc != RM_EOF)
        return (rc);

    printf("%d records found\n", n);

    // Return ok
    return (0);
}


//
// UpdateRecords
//
// Desc: Update the records in a file
//
RC UpdateRecords(RM_FileHandle &fh, int numRecs)
{
    RC        rc;
    int       n;
    RM_Record rec;

    printf("\nUpdating records in the file\n");

    for (int i=1; i<=10; i++) {
        RID rid(1, i);
        if ((rc = GetRec(fh, rid, rec))) {
            return rc;
        }
        char* pData;
        if ((rc = rec.GetData(pData))) {
            return rc;
        }
        TestRec* tempRecord = (TestRec*) pData;
        tempRecord->num = 2000;

        // Update record
        if ((rc = UpdateRec(fh, rec))) {
            return rc;
        }
    }

    // Return ok
    rc = 0;

    return rc;
}

//
// DeleteRecords
//
// Desc: Delete the records in a file
//
RC DeleteRecords(RM_FileHandle &fh, int numRecs)
{
    RC        rc;
    int       n;
    RM_Record rec;

    printf("\nDeleting records in the file\n");

    for (int i=1; i<=10; i++) {
        RID rid(1, i);

        // Delete record
        if ((rc = DeleteRec(fh, rid))) {
            return rc;
        }
    }

    // Return ok
    rc = 0;

    return rc;
}



////////////////////////////////////////////////////////////////////////
// The following functions are wrappers for some of the RM component  //
// methods.  They give you an opportunity to add debugging statements //
// and/or set breakpoints when testing these methods.                 //
////////////////////////////////////////////////////////////////////////

//
// CreateFile
//
// Desc: call RM_Manager::CreateFile
//
RC CreateFile(char *fileName, int recordSize)
{
    printf("\ncreating %s\n", fileName);
    return (rmm.CreateFile(fileName, recordSize));
}

//
// DestroyFile
//
// Desc: call RM_Manager::DestroyFile
//
RC DestroyFile(char *fileName)
{
    printf("\ndestroying %s\n", fileName);
    return (rmm.DestroyFile(fileName));
}

//
// OpenFile
//
// Desc: call RM_Manager::OpenFile
//
RC OpenFile(char *fileName, RM_FileHandle &fh)
{
    printf("\nopening %s\n", fileName);
    return (rmm.OpenFile(fileName, fh));
}

//
// CloseFile
//
// Desc: call RM_Manager::CloseFile
//
RC CloseFile(char *fileName, RM_FileHandle &fh)
{
    if (fileName != NULL)
        printf("\nClosing %s\n", fileName);
    return (rmm.CloseFile(fh));
}

//
// GetRec
//
// Desc: call RM_FileHandle::GetRec
//
RC GetRec(RM_FileHandle &fh, RID &rid, RM_Record &rec)
{
    return (fh.GetRec(rid, rec));
}

//
// InsertRec
//
// Desc: call RM_FileHandle::InsertRec
//
RC InsertRec(RM_FileHandle &fh, char *record, RID &rid)
{
    return (fh.InsertRec(record, rid));
}

//
// DeleteRec
//
// Desc: call RM_FileHandle::DeleteRec
//
RC DeleteRec(RM_FileHandle &fh, RID &rid)
{
    return (fh.DeleteRec(rid));
}

//
// UpdateRec
//
// Desc: call RM_FileHandle::UpdateRec
//
RC UpdateRec(RM_FileHandle &fh, RM_Record &rec)
{
    return (fh.UpdateRec(rec));
}

//
// GetNextRecScan
//
// Desc: call RM_FileScan::GetNextRec
//
RC GetNextRecScan(RM_FileScan &fs, RM_Record &rec)
{
    return (fs.GetNextRec(rec));
}

/////////////////////////////////////////////////////////////////////
// Sample test functions follow.                                   //
/////////////////////////////////////////////////////////////////////

//
// Test1 tests simple creation, opening, closing, and deletion of files
//
RC Test1(void)
{
    RC            rc;
    RM_FileHandle fh;

    printf("\ntest1 starting\n*****************************\n");

    if ((rc = CreateFile(FILENAME, sizeof(TestRec))) ||
        (rc = OpenFile(FILENAME, fh)) ||
        (rc = CloseFile(FILENAME, fh)))
        return (rc);

    LsFile(FILENAME);

    if ((rc = DestroyFile(FILENAME)))
        return (rc);

    printf("\ntest1 done\n*****************************\n");
    return (0);
}

//
// Test2 tests adding a few records to a file.
//
RC Test2(void)
{
    RC            rc;
    RM_FileHandle fh;

    printf("\ntest2 starting\n*****************************\n");

    if ((rc = CreateFile(FILENAME, sizeof(TestRec))) ||
        (rc = OpenFile(FILENAME, fh)) ||
        (rc = AddRecs(fh, FEW_RECS)) ||
        (rc = CloseFile(FILENAME, fh)))
        return (rc);

    LsFile(FILENAME);

    if ((rc = DestroyFile(FILENAME)))
        return (rc);

    printf("\ntest2 done\n*****************************\n");
    return (0);
}

//
// Test3 verifies that records are added to a file.
//
RC Test3(void)
{
    RC            rc;
    RM_FileHandle fh;

    printf("\ntest3 starting\n*****************************\n");

    if ((rc = CreateFile(FILENAME, sizeof(TestRec)))) {
        return rc;
    }
    if ((rc = OpenFile(FILENAME, fh))) {
        return rc;
    }
    if ((rc = AddRecs(fh, FEW_RECS))) {
        return rc;
    }
    if ((rc = CloseFile(FILENAME, fh))) {
        return (rc);
    }

    // Verify file
    if ((rc = OpenFile(FILENAME, fh))) {
        return rc;
    }
    if ((rc = VerifyFile(fh, FEW_RECS, NO_OP))) {
        return rc;
    }
    if ((rc = CloseFile(FILENAME, fh))) {
        return (rc);
    }

    LsFile(FILENAME);

    if ((rc = DestroyFile(FILENAME)))
        return (rc);

    printf("\ntest3 done\n*****************************\n");
    return (0);
}


//
// Test4 tests updating of records in a file
//
RC Test4(void)
{
    RC            rc;
    RM_FileHandle fh;

    printf("\ntest4 starting\n*****************************\n");

    if ((rc = CreateFile(FILENAME, sizeof(TestRec)))) {
        return rc;
    }
    if ((rc = OpenFile(FILENAME, fh))) {
        return rc;
    }
    if ((rc = AddRecs(fh, FEW_RECS))) {
        return rc;
    }
    if ((rc = VerifyFile(fh, FEW_RECS, LT_OP))) {
        return rc;
    }
    if ((rc = CloseFile(FILENAME, fh))) {
        return (rc);
    }

    printf("\n---------------------------------\n");

    // Update records in the file
    if ((rc = OpenFile(FILENAME, fh))) {
        return rc;
    }
    if ((rc = UpdateRecords(fh, FEW_RECS))) {
        return rc;
    }
    if ((rc = VerifyFile(fh, FEW_RECS, LT_OP))) {
        return rc;
    }
    if ((rc = CloseFile(FILENAME, fh))) {
        return (rc);
    }

    LsFile(FILENAME);

    if ((rc = DestroyFile(FILENAME)))
        return (rc);

    printf("\ntest4 done\n*****************************\n");
    return (0);
}

//
// Test5 tests deleting of records in a file
//
RC Test5(void)
{
    RC            rc;
    RM_FileHandle fh;

    printf("\ntest5 starting\n*****************************\n");

    if ((rc = CreateFile(FILENAME, sizeof(TestRec)))) {
        return rc;
    }
    if ((rc = OpenFile(FILENAME, fh))) {
        return rc;
    }
    if ((rc = AddRecs(fh, FEW_RECS))) {
        return rc;
    }
    if ((rc = VerifyFile(fh, FEW_RECS, NO_OP))) {
        return rc;
    }
    if ((rc = CloseFile(FILENAME, fh))) {
        return (rc);
    }

    printf("\n---------------------------------\n");

    // Delete records in the file
    if ((rc = OpenFile(FILENAME, fh))) {
        return rc;
    }
    if ((rc = DeleteRecords(fh, FEW_RECS))) {
        return rc;
    }
    if ((rc = VerifyFile(fh, FEW_RECS, NO_OP))) {
        return rc;
    }
    if ((rc = CloseFile(FILENAME, fh))) {
        return (rc);
    }

    LsFile(FILENAME);

    if ((rc = DestroyFile(FILENAME)))
        return (rc);

    printf("\ntest5 done\n*****************************\n");
    return (0);
}