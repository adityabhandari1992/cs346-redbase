//
// ql.h
//   Query Language Component Interface
//

// This file only gives the stub for the QL component

#ifndef QL_H
#define QL_H

#include <stdlib.h>
#include <string.h>
#include "redbase.h"
#include "parser.h"
#include "rm.h"
#include "ix.h"
#include "sm.h"

//
// QL_Manager: query language (DML)
//
class QL_Manager {
public:
    QL_Manager (SM_Manager &smm, IX_Manager &ixm, RM_Manager &rmm);
    ~QL_Manager();                       // Destructor

    RC Select  (int nSelAttrs,           // # attrs in select clause
        const RelAttr selAttrs[],        // attrs in select clause
        int   nRelations,                // # relations in from clause
        const char * const relations[],  // relations in from clause
        int   nConditions,               // # conditions in where clause
        const Condition conditions[]);   // conditions in where clause

    RC Insert  (const char *relName,     // relation to insert into
        int   nValues,                   // # values
        const Value values[]);           // values to insert

    RC Delete  (const char *relName,     // relation to delete from
        int   nConditions,               // # conditions in where clause
        const Condition conditions[]);   // conditions in where clause

    RC Update  (const char *relName,     // relation to update
        const RelAttr &updAttr,          // attribute to update
        const int bIsValue,              // 1 if RHS is a value, 0 if attribute
        const RelAttr &rhsRelAttr,       // attr on RHS to set LHS equal to
        const Value &rhsValue,           // or value to set attr equal to
        int   nConditions,               // # conditions in where clause
        const Condition conditions[]);   // conditions in where clause

private:
    RM_Manager* rmManager;          // RM_Manager object
    IX_Manager* ixManager;          // IX_Manager object
    SM_Manager* smManager;         // SM_Manager object
};

//
// Print-error function
//
void QL_PrintError(RC rc);

// Warnings
#define QL_DATABASE_DOES_NOT_EXIST          (START_QL_WARN + 0) // Database does not exist
#define QL_LASTWARN                         QL_DATABASE_DOES_NOT_EXIST

// Errors
#define QL_INVALID_DATABASE_NAME            (START_QL_ERR - 0) // Invalid database file name

// Error in UNIX system call or library routine
#define QL_UNIX                 (START_QL_ERR - 1) // Unix error
#define QL_LASTERROR            QL_UNIX


#endif
