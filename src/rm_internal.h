//
// File:        rm_internal.h
// Description: Declarations internal to the RM component
// Authors:     Aditya Bhandari (adityasb@stanford.edu)
//

#ifndef RM_INTERNAL_H
#define RM_INTERNAL_H

#include <string>
#include "rm.h"

// Constants and defines
#define RM_NO_FREE_PAGE    -1  // Like a null pointer for the free list

// Data Structures

// RM_PageHeader: Struct for the page header
/* Stores the following:
    1) Pointer to the next free page - PageNum
*/
struct RM_PageHeader {
    PageNum nextPage;
};

#endif