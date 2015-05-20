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
    SM_Manager* smManager;          // SM_Manager object

    RC GetAttrInfoFromArray(char* attributes, int attrCount, const char* attrName, char* attributeData);

    template <typename T>
    bool matchRecord(T lhsValue, T rhsValue, CompOp op);

    const char* OperatorToString(CompOp op);
    void GetValue(const Value* v, std::string& queryPlan);
    void AddScanToQueryPlan(std::string scanType, const char* relName, bool cond, char* attrName, CompOp op, const Value* v, std::string& queryPlan);
};

//
// Print-error function
//
void QL_PrintError(RC rc);

// Warnings
#define QL_DATABASE_DOES_NOT_EXIST          (START_QL_WARN + 0) // Database does not exist
#define QL_DATABASE_CLOSED                  (START_QL_WARN + 1) // Database is closed
#define QL_NULL_RELATION                    (START_QL_WARN + 2) // Null relation name
#define QL_SYSTEM_CATALOG                   (START_QL_WARN + 3) // System catalog
#define QL_INCORRECT_INDEX_COUNT            (START_QL_WARN + 4) // Incorrect index count
#define QL_INCORRECT_ATTR_COUNT             (START_QL_WARN + 5) // Incorrect attribute count
#define QL_INCORRECT_ATTRIBUTE_TYPE         (START_QL_WARN + 6) // Incorrect attribute type
#define QL_INVALID_CONDITION                (START_QL_WARN + 7) // Invalid condition
#define QL_ATTRIBUTE_NOT_FOUND              (START_QL_WARN + 8) // Attribute not found
#define QL_INVALID_UPDATE_ATTRIBUTE         (START_QL_WARN + 9) // Invalid update attribute
#define QL_LASTWARN                         QL_INVALID_UPDATE_ATTRIBUTE

// Errors
#define QL_INVALID_DATABASE_NAME            (START_QL_ERR - 0) // Invalid database file name

// Error in UNIX system call or library routine
#define QL_UNIX                 (START_QL_ERR - 1) // Unix error
#define QL_LASTERROR            QL_UNIX


#endif
