//
// File:        ix_error.cc
// Description: IX_PrintError implementation
// Authors:     Aditya Bhandari (adityasb@stanford.edu)
//

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <iostream>
#include "ix_internal.h"
#include "ix.h"

using namespace std;

//
// Error table
//
static char *IX_WarnMsg[] = {
  (char*)"record size is too large",
  (char*)"record size is too small"
};

static char *IX_ErrorMsg[] = {
  (char*)"invalid file name",
  (char*)"inconsistent bitmap on file page"
};

//
// IX_PrintError
//
// Desc: Send a message corresponding to an IX return code to cerr
// In:   rc - return code for which a message is desired
//
void IX_PrintError(RC rc)
{
  // Check the return code is within proper limits
  if (rc >= START_IX_WARN && rc <= IX_LASTWARN)
    // Print warning
    cerr << "IX warning: " << IX_WarnMsg[rc - START_IX_WARN] << "\n";
  // Error codes are negative, so invert everything
  else if (-rc >= -START_IX_ERR && -rc < -IX_LASTERROR)
    // Print error
    cerr << "IX error: " << IX_ErrorMsg[-rc + START_IX_ERR] << "\n";
  else if (rc == IX_UNIX)
#ifdef PC
      cerr << "OS error\n";
#else
      cerr << strerror(errno) << "\n";
#endif
  else if (rc == 0)
    cerr << "IX_PrintError called with return code of 0\n";
  else
    cerr << "IX error: " << rc << " is out of bounds\n";
}
