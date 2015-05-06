//
// dbdestroy.cc
//
// Author: Aditya Bhandari (adityasb@stanford.edu)
//

#include <iostream>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include "sm.h"
#include "redbase.h"

using namespace std;

//
// main
//
/* Steps:
    1) Remove the directory for the database
*/
int main(int argc, char *argv[])
{
    char *dbname;
    char command[255] = "rm -r ";

    // Look for 2 arguments. The first is always the name of the program
    // that was executed, and the second should be the name of the
    // database.
    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " dbname \n";
        exit(1);
    }

    // The database name is the second argument
    dbname = argv[1];

    // Remove the subdirectory for the database
    if (system (strcat(command,dbname)) != 0) {
        cerr << argv[0] << " cannot destroy the database: " << dbname << "\n";
        exit(1);
    }

    return 0;
}
