//
// File:        ql_error.cc
// Description: QL_PrintError implementation
// Authors:     Aditya Bhandari (adityasb@stanford.edu)
//

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <iostream>
#include "ql.h"

using namespace std;

static char *QL_WarnMsg[] = {
  (char*)"database does not exist",
  (char*)"database is closed",
  (char*)"null relation name",
  (char*)"cannot change system catalog",
  (char*)"incorrect index count",
  (char*)"incorrect attribute count",
  (char*)"incorrect attribute type",
  (char*)"invalid condition",
  (char*)"attribute not found",
  (char*)"invalid update attribute"
};

static char *QL_ErrorMsg[] = {
  (char*)"invalid database name"
};

//
// QL_PrintError
//
// Desc: Send a message corresponding to a QL return code to cerr
// In:   rc - return code for which a message is desired
//
void QL_PrintError(RC rc)
{
  // Check the return code is within proper limits
  if (rc >= START_QL_WARN && rc <= QL_LASTWARN)
    // Print warning
    cerr << "QL warning: " << QL_WarnMsg[rc - START_QL_WARN] << "\n";
  // Error codes are negative, so invert everything
  else if (-rc >= -START_QL_ERR && -rc < -QL_LASTERROR)
    // Print error
    cerr << "QL error: " << QL_ErrorMsg[-rc + START_QL_ERR] << "\n";
  else if (rc == QL_UNIX)
#ifdef PC
      cerr << "OS error\n";
#else
      cerr << strerror(errno) << "\n";
#endif
  else if (rc == 0)
    cerr << "QL_PrintError called with return code of 0\n";
  else
    cerr << "QL error: " << rc << " is out of bounds\n";
}
