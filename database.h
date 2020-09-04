/* ODBC database library for C */
/* Wrapping and simplifying ODBC 3.0 into higher level functions */
/* Use only from a single thread */
/* Version 0.5 - 2018-10 - Thomas Munk */

#ifndef DATABASE_H
#define DATABASE_H

#include <time.h>

#define DB_CALLBACK(F) int (*F)(void)

/* Common error string where functions write error messages. */
extern char db_errorstr[1024];

/* Set connection timeout in seconds. 0 (default) selects system default value. */
extern unsigned short db_conntimeout;

/* Connect to database. After this always call db_disconnect, even when failing. */
/*Return FALSE (0) on error */
extern char db_connect(char *connstr);

/* Disconnect from database. Can always be called */
extern void db_disconnect();

/* Determine if database connection exists. */
//TODO-DOC: Hvis systemet tror det er connected (men connection er gået tabt) skal der en nedenstående handling til (som fejler) før der returneres false
extern char db_isconnected();

/* Perform updating SQL (INSERT, UPDATE, DELETE). Never return records, but affected number of rows can be returned */
/* Return FALSE (0) on error */
extern char db_sqlwrite(char *sqlstr, int *affrows);

/* Perform selective SQL (SELECT). Callback function cb_start is called once and cb_row is called for every returned record */
/* Return 1 from callback functions to continue, otherwise db_sqlread is terminated */
/* Return FALSE (0) on error */
extern char db_sqlread(char *sqlstr, DB_CALLBACK(cb_start), DB_CALLBACK(cb_row));

/* The following functions (db_sqlread_*) can only be used from inside db_sqlread's callback functions. They operate on the actual row. */
/* There is no return values to indicate errors but db_errorstr kan be cleared before and checked after calling for still empty, "NULL" or error string. */

/* Get total number of records. Not all data sources can return this and -1 or an inaccurate number can be returned. Always expect this. */
extern long db_sqlread_rowcount();

/* Get total number of columns. Normally only needed for SQL with SELECT * and the like */
extern short db_sqlread_colcount();

/* Get data type of column ('i', 'f', 's', 't') as a best fit for selecting one of the four next functions */
/* Column numbers starts from 1 */
extern char db_sqlread_coltype(short colno);

/* Read a field value as integer (i), float (f), string (s) or date/time (t) */
extern long db_sqlread_i(short colno);
extern double db_sqlread_f(short colno);
extern char* db_sqlread_s(short colno);
extern time_t db_sqlread_t(short colno);

#endif /* DATABASE_H */