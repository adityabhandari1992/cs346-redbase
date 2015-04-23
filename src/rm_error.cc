//
// File:        rm_error.cc
// Description: RM_PrintError implementation
// Authors:     Aditya Bhandari (adityasb@stanford.edu)
//

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string>
#include <iostream>
#include "rm_internal.h"
#include "rm.h"

using namespace std;

//
// Error table
//
static char *RM_WarnMsg[] = {
  (char*)"record size is too large",
  (char*)"record size is too small",
  (char*)"file is already open",
  (char*)"file is closed",
  (char*)"record is not valid",
  (char*)"slot number is not valid",
  (char*)"page number is not valid",
  (char*)"attributes are not consistent",
  (char*)"scan is not open",
  (char*)"file name is not valid",
  (char*)"attribute type is not valid",
  (char*)"attribute offset is not valid",
  (char*)"operator is not valid",
  (char*)"record is null pointer",
  (char*)"end of file",
};

static char *RM_ErrorMsg[] = {
  (char*)"invalid file name",
  (char*)"inconsistent bitmap on file page"
};

//
// RM_PrintError
//
// Desc: Send a message corresponding to an RM return code to cerr
// In:   rc - return code for which a message is desired
//
void RM_PrintError(RC rc)
{
  // Check the return code is within proper limits
  if (rc >= START_RM_WARN && rc <= RM_LASTWARN)
    // Print warning
    cerr << "RM warning: " << RM_WarnMsg[rc - START_RM_WARN] << "\n";
  // Error codes are negative, so invert everything
  else if (-rc >= -START_RM_ERR && -rc < -RM_LASTERROR)
    // Print error
    cerr << "RM error: " << RM_ErrorMsg[-rc + START_RM_ERR] << "\n";
  else if (rc == RM_UNIX)
#ifdef PC
      cerr << "OS error\n";
#else
      cerr << strerror(errno) << "\n";
#endif
  else if (rc == 0)
    cerr << "RM_PrintError called with return code of 0\n";
  else
    cerr << "RM error: " << rc << " is out of bounds\n";
}