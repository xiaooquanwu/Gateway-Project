#ifndef _SQLITE_H_H_
#define _SQLITE_H_H_

#include <stdio.h>
#include <sqlite3.h>
#include <stdlib.h>

void Open_db(const char *filename, sqlite3 **ppDb);
void Insert_db(sqlite3 **ppDb, char *msg);


#endif

