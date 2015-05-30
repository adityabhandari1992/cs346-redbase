//
// ex.h
//   EX Component Interface
//

#ifndef EX_H
#define EX_H

#include <stdlib.h>
#include <string.h>
#include "redbase.h"
#include "parser.h"
#include "printer.h"
#include "rm.h"
#include "ix.h"
#include "sm.h"

// Data structures

// EX_DbInfo - Database information stored for each database
/* Stores the following:
    1) distributed - whether the database is distributed - integer
    2) numberNodes - number of nodes - integer
*/
struct EX_DBInfo {
    int distributed;
    int numberNodes;
};

// EX_IntPartitionVectorRecord - Record for int partition vector entries
/* Stores the following:
    1) node - node number - integer
    2) startValue - start value for partition - integer
    3) endValue - end value for partition - integer
*/
struct EX_IntPartitionVectorRecord {
    int node;
    int startValue;
    int endValue;
};

// EX_FloatPartitionVectorRecord - Record for float partition vector entries
/* Stores the following:
    1) node - node number - integer
    2) startValue - start value for partition - float
    3) endValue - end value for partition - float
*/
struct EX_FloatPartitionVectorRecord {
    int node;
    float startValue;
    float endValue;
};

// EX_StringPartitionVectorRecord - Record for string partition vector entries
/* Stores the following:
    1) node - node number - integer
    2) startValue - start value for partition - char*
    3) endValue - end value for partition - char*
*/
struct EX_StringPartitionVectorRecord {
    int node;
    char startValue[MAXSTRINGLEN+1];
    char endValue[MAXSTRINGLEN+1];
};

// Constants
#define EX_DBINFO_ATTR_COUNT            2
#define EX_PARTITION_VECTOR_ATTR_COUNT  3

// Maximum values for keys
#define MAX_INT     99999999
#define MAX_FLOAT   99999999.0
#define MAX_STRING  "zzzzzzzz"


// EX_CommLayer class
// Class to simulate the communication layer for the distributed redbase
class EX_CommLayer {
public:
    EX_CommLayer(RM_Manager* rmm, IX_Manager* ixm);
    ~EX_CommLayer();

    RC CreateTableInDataNode(const char* relName, int attrCount, AttrInfo* attributes, int node);
    RC DropTableInDataNode(const char* relName, int node);
    RC PrintInDataNode(Printer &p, const char* relName, int node);

private:
    RM_Manager* rmManager;
    IX_Manager* ixManager;
    SM_Manager* smManager;
    QL_Manager* qlManager;
};


//
// Print-error function
//
void EX_PrintError(RC rc);

// Warnings
#define EX_INCORRECT_VALUE_COUNT            (START_EX_WARN + 0) // Incorrect number of values
#define EX_INVALID_ATTRIBUTE                (START_EX_WARN + 1) // Invalid attribute name
#define EX_INVALID_VALUE                    (START_EX_WARN + 2) // Invalid value
#define EX_INVALID_DATA_NODE                (START_EX_WARN + 3) // Invalid data node
#define EX_LASTWARN                         EX_INVALID_VALUE

// Errors
#define EX_UNIX                             (START_EX_ERR - 0) // Unix error
#define EX_LASTERROR                        EX_UNIX

#endif
