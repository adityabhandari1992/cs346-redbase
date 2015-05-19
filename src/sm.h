//
// sm.h
//   Data Manager Component Interface
//

#ifndef SM_H
#define SM_H

// Please do not include any other files than the ones below in this file.

#include <stdlib.h>
#include <string.h>
#include "redbase.h"  // Please don't change these lines
#include "parser.h"
#include "rm.h"
#include "ix.h"

// Data structures

// SM_RelcatRecord - Records stored in the relcat relation
/* Stores the follwing:
    1) relName - name of the relation - char*
    2) tupleLength - length of the tuples - integer
    3) attrCount - number of attributes - integer
    4) indexCount - number of indexes - integer
*/
struct SM_RelcatRecord {
    char relName[MAXNAME+1];
    int tupleLength;
    int attrCount;
    int indexCount;
};

// SM_AttrcatRecord - Records stored in the attrcat relation
/* Stores the follwing:
    1) relName - name of the relation - char*
    2) attrName - name of the attribute - char*
    3) offset - offset of the attribute - integer
    4) attrType - type of the attribute - AttrType
    5) attrLength - length of the attribute - integer
    6) indexNo - number of the index - integer
*/
struct SM_AttrcatRecord {
    char relName[MAXNAME+1];
    char attrName[MAXNAME+1];
    int offset;
    AttrType attrType;
    int attrLength;
    int indexNo;
};

// Constants
#define SM_RELCAT_ATTR_COUNT    4
#define SM_ATTRCAT_ATTR_COUNT   6

//
// SM_Manager: provides data management
//
class SM_Manager {
    friend class QL_Manager;
public:
    SM_Manager    (IX_Manager &ixm, RM_Manager &rmm);
    ~SM_Manager   ();                             // Destructor

    RC OpenDb     (const char *dbName);           // Open the database
    RC CloseDb    ();                             // close the database

    RC CreateTable(const char *relName,           // create relation relName
                   int        attrCount,          //   number of attributes
                   AttrInfo   *attributes);       //   attribute data
    RC CreateIndex(const char *relName,           // create an index for
                   const char *attrName);         //   relName.attrName
    RC DropTable  (const char *relName);          // destroy a relation

    RC DropIndex  (const char *relName,           // destroy index on
                   const char *attrName);         //   relName.attrName
    RC Load       (const char *relName,           // load relName from
                   const char *fileName);         //   fileName
    RC Help       ();                             // Print relations in db
    RC Help       (const char *relName);          // print schema of relName

    RC Print      (const char *relName);          // print relName contents

    RC Set        (const char *paramName,         // set parameter to
                   const char *value);            //   value

    // Methods to get the tuples from the system catalogs
    RC GetAttrInfo(const char* relName, int attrCount, char* attributeData);
    RC GetAttrInfo(const char* relName, const char* attrName, SM_AttrcatRecord* attributeData);
    RC GetRelInfo(const char* relName, SM_RelcatRecord* relationData);

    int getPrintFlag();             // Method to get the printCommands flag
    int getOpenFlag();              // Method to get the isOpen flag

private:
    RM_Manager* rmManager;          // RM_Manager object
    IX_Manager* ixManager;          // IX_Manager object

    RM_FileHandle relcatFH;         // RM file handle for relcat
    RM_FileHandle attrcatFH;        // RM file handle for attrcat
    int isOpen;                     // Flag whether the database is open

    int printCommands;              // System parameter specifying printing level
};

//
// Print-error function
//
void SM_PrintError(RC rc);


// Warnings
#define SM_DATABASE_DOES_NOT_EXIST          (START_SM_WARN + 0) // Database does not exist
#define SM_INVALID_DATABASE_CLOSE           (START_SM_WARN + 1) // Database cannot be closed
#define SM_DATABASE_OPEN                    (START_SM_WARN + 2) // Database is open
#define SM_DATABASE_CLOSED                  (START_SM_WARN + 3) // Database is closed
#define SM_INCORRECT_ATTRIBUTE_COUNT        (START_SM_WARN + 4) // Attribute count is wrong
#define SM_NULL_ATTRIBUTES                  (START_SM_WARN + 5) // Null attribute pointer
#define SM_INVALID_NAME                     (START_SM_WARN + 6) // Invalid name
#define SM_TABLE_DOES_NOT_EXIST             (START_SM_WARN + 7) // Table does not exist
#define SM_TABLE_ALREADY_EXISTS             (START_SM_WARN + 8) // Table already exists
#define SM_NULL_RELATION                    (START_SM_WARN + 9) // Null relation name
#define SM_NULL_FILENAME                    (START_SM_WARN + 10) // Null file name
#define SM_INVALID_DATA_FILE                (START_SM_WARN + 11) // Invalid data file
#define SM_INCORRECT_INDEX_COUNT            (START_SM_WARN + 12) // Incorrect index count
#define SM_NULL_PARAMETERS                  (START_SM_WARN + 13) // Null parameters
#define SM_INVALID_SYSTEM_PARAMETER         (START_SM_WARN + 14) // Invalid system parameter
#define SM_INVALID_VALUE                    (START_SM_WARN + 15) // Invalid value
#define SM_INDEX_EXISTS                     (START_SM_WARN + 16) // Index already exists
#define SM_INDEX_DOES_NOT_EXIST             (START_SM_WARN + 17) // Index does not exist
#define SM_SYSTEM_CATALOG                   (START_SM_WARN + 18) // Cannot change system catalog
#define SM_LASTWARN                         SM_SYSTEM_CATALOG

// Errors
#define SM_INVALID_DATABASE_NAME            (START_SM_ERR - 0) // Invalid database file name

// Error in UNIX system call or library routine
#define SM_UNIX                 (START_SM_ERR - 1) // Unix error
#define SM_LASTERROR            SM_UNIX

#endif
