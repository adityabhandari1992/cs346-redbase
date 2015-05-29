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
#include "rm.h"
#include "ix.h"
#include "sm.h"
#include "ql.h"

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

// Constants
#define EX_DBINFO_ATTR_COUNT    2

//
// Print-error function
//
void EX_PrintError(RC rc);

// Warnings
#define EX_INCORRECT_VALUE_COUNT            (START_EX_WARN + 0) // Incorrect number of values
#define EX_INVALID_ATTRIBUTE                (START_EX_WARN + 1) // Invalid attribute name
#define EX_INVALID_VALUE                    (START_EX_WARN + 2) // Invalid value
#define EX_LASTWARN                         EX_INVALID_VALUE

// Errors
#define EX_UNIX                             (START_EX_ERR - 0) // Unix error
#define EX_LASTERROR                        EX_UNIX

#endif
