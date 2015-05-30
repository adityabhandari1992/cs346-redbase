//
// File:        ex_error.cc
// Description: EX_PrintError implementation
// Authors:     Aditya Bhandari (adityasb@stanford.edu)
//

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <iostream>
#include "ex.h"

using namespace std;

static char *EX_WarnMsg[] = {
  (char*)"incorrect number of data values",
  (char*)"invalid attribute name",
  (char*)"invalid value in partition vector",
  (char*)"invalid data node"
};

static char *EX_ErrorMsg[] = {
};

//
// EX_PrintError
//
// Desc: Send a message corresponding to an EX return code to cerr
// In:   rc - return code for which a message is desired
//
void EX_PrintError(RC rc)
{
  // Check the return code is within proper limits
  if (rc >= START_EX_WARN && rc <=EX_LASTWARN)
    // Print warning
    cerr << "EX warning: " << EX_WarnMsg[rc - START_EX_WARN] << "\n";
  // Error codes are negative, so invert everything
  else if (-rc >= -START_EX_ERR && -rc < -EX_LASTERROR)
    // Print error
    cerr << "EX error: " << EX_ErrorMsg[-rc + START_EX_ERR] << "\n";
  else if (rc == EX_UNIX)
#ifdef PC
      cerr << "OS error\n";
#else
      cerr << strerror(errno) << "\n";
#endif
  else if (rc == 0)
    cerr << "EX_PrintError called with return code of 0\n";
  else
    cerr << "EX error: " << rc << " is out of bounds\n";
}
