#include "sqlite.h"
#include <time.h>

static int callback(void *NotUsed, int argc, char **argv, char **azColName)
{
   int i;
   for(i=0; i<argc; i++)
      printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
      
   printf("\n");
   return 0;
}

void Open_db(const char *filename, sqlite3 **ppDb)
{
	int rc;
	
	rc = sqlite3_open(filename, ppDb);
	if( rc )
	{
		fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(*ppDb));
		exit(0);
	}
	else
	{
		fprintf(stdout, "Opened database successfully\n");
	}
}

void Insert_db(sqlite3 **ppDb, char *msg)
{
	char *zErrMsg = 0;
	int rc;
	char sql[512] = {'\0'};
	time_t timep;
	
	//获取当前时间
	time (&timep);
	
	/* Create SQL statement */
	sprintf(sql, "INSERT INTO COAPMSG (TIME,ID,MSG) VALUES ('%s', NULL, '%s'); ", ctime(&timep), msg);
	
	/* Execute SQL statement */
	rc = sqlite3_exec(*ppDb, sql, callback, 0, &zErrMsg);
	if( rc != SQLITE_OK )
	{
		fprintf(stderr, "SQL error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
	}
	else
	{
		//fprintf(stdout, "Records created successfully\n");
	}
	
}

int test(int argc, char** argv)
{
	sqlite3 *db;
	
	//打开数据库
	Open_db("CoapMsg.db", &db);
	
	//插入一条数据
	Insert_db(&db, "hello");

	return 0;
}


