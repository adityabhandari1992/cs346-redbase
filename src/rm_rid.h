//
// rm_rid.h
//
//   The Record Id interface
//

#ifndef RM_RID_H
#define RM_RID_H

// We separate the interface of RID from the rest of RM because some
// components will require the use of RID but not the rest of RM.

#include "redbase.h"

//
// PageNum: uniquely identifies a page in a file
//
typedef int PageNum;

//
// SlotNum: uniquely identifies a record in a page
//
typedef int SlotNum;

//
// RID: Record id interface
//
class RID {
public:
    RID();                                         // Default constructor
    RID(PageNum pageNum, SlotNum slotNum);
    ~RID();                                        // Destructor

   RID  (const RID &rid);                          // Copy constructor

   RID& operator=(const RID &rid);                 // Overload =


    RC GetPageNum(PageNum &pageNum) const;         // Return page number
    RC GetSlotNum(SlotNum &slotNum) const;         // Return slot number

private:
    PageNum pageNumber;         // Page number
    SlotNum slotNumber;         // Slot number
    int isViable;               // Viable flag
};

// Error codes
#define RID_NOT_VIABLE      (START_RM_WARN + 0) // RID is not viable


#endif
