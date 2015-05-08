//
// File:        sm_error.cc
// Description: SM_PrintError implementation
// Authors:     Aditya Bhandari (adityasb@stanford.edu)
//

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <iostream>
#include "sm.h"

using namespace std;

static char *SM_WarnMsg[] = {
  (char*)"database does not exist",
  (char*)"database cannot be closed",
  (char*)"database is already open",
  (char*)"database is closed",
  (char*)"attribute count is wrong",
  (char*)"null attribute pointer",
  (char*)"invalid user-specified name",
  (char*)"relation does not exist",
  (char*)"relation exists",
  (char*)"null relation name",
  (char*)"null file name",
  (char*)"invalid data file",
  (char*)"incorrect index count",
  (char*)"null parameters",
  (char*)"invalid system parameter",
  (char*)"invalid parameter value",
  (char*)"index already exists",
  (char*)"index does not exist",
  (char*)"cannot change system catalog"
};

static char *SM_ErrorMsg[] = {
  (char*)"invalid database name"
};

//
// SM_PrintError
//
// Desc: Send a message corresponding to an SM return code to cerr
// In:   rc - return code for which a message is desired
//
void SM_PrintError(RC rc)
{
  // Check the return code is within proper limits
  if (rc >= START_SM_WARN && rc <= SM_LASTWARN)
    // Print warning
    cerr << "SM warning: " << SM_WarnMsg[rc - START_SM_WARN] << "\n";
  // Error codes are negative, so invert everything
  else if (-rc >= -START_SM_ERR && -rc < -SM_LASTERROR)
    // Print error
    cerr << "SM error: " << SM_ErrorMsg[-rc + START_SM_ERR] << "\n";
  else if (rc == SM_UNIX)
#ifdef PC
      cerr << "OS error\n";
#else
      cerr << strerror(errno) << "\n";
#endif
  else if (rc == 0)
    cerr << "SM_PrintError called with return code of 0\n";
  else
    cerr << "SM error: " << rc << " is out of bounds\n";
}
