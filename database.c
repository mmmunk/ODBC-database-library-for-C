/* ODBC database library for C */
/* Wrapping and simplifying ODBC 3.0 into higher level functions */
/* Use only from a single thread */
/* Version 0.5 - 2018-10 - Thomas Munk */

#include <stdio.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#else
#define FALSE 0
#define TRUE 1
#endif
#include <sql.h>
#include <sqlext.h>
#include "database.h"

// ------------------------------ ERROR ------------------------------

char db_errorstr[1024] = "";

void generate_errstr(char *text, SQLSMALLINT htype, SQLHANDLE handle) {
	strcpy(db_errorstr, text);
	if (htype && handle) {
		SQLCHAR state[6], msg[512];
		SQLINTEGER nep;
		SQLSMALLINT msglen;
		SQLRETURN status = SQLGetDiagRec(htype, handle, 1, state, &nep, msg, sizeof msg, &msglen);
		if (status == SQL_SUCCESS || status == SQL_SUCCESS_WITH_INFO) {
			int n = strlen(db_errorstr) + 2 + 5 + 13 + msglen;
			if (n >= sizeof db_errorstr) msg[msglen - (n - sizeof db_errorstr) - 1] = 0;
			sprintf(&db_errorstr[strlen(db_errorstr)], ": %s %ld %s", state, (long)nep, msg);
		}
	}
}

// ------------------------------ CONNECTION ------------------------------

SQLHENV environment = NULL;
SQLHDBC connection = NULL;
char connsuccess = FALSE;
unsigned short db_conntimeout = 0;

char connect_conditions_ok() {
	return connsuccess && connection && environment;
}

void db_disconnect() {
	if (connsuccess) {
		SQLDisconnect(connection);
		connsuccess = FALSE;
	}
	if (connection) {
		SQLFreeHandle(SQL_HANDLE_DBC, connection);
		connection = NULL;
	}
	if (environment) {
		SQLFreeHandle(SQL_HANDLE_ENV, environment);
		environment = NULL;
	}
}

char db_connect(char *connstr) {
	db_disconnect();
	SQLRETURN status = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &environment);
	if (status != SQL_SUCCESS && status != SQL_SUCCESS_WITH_INFO) {
		generate_errstr("Error allocating environment", 0, 0);
		return FALSE;
	}
	status = SQLSetEnvAttr(environment, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0);
	if (status != SQL_SUCCESS && status != SQL_SUCCESS_WITH_INFO) {
		generate_errstr("Error setting environment ODBC version", SQL_HANDLE_ENV, environment);
		return FALSE;
	}
	status = SQLAllocHandle(SQL_HANDLE_DBC, environment, &connection);
	if (status != SQL_SUCCESS && status != SQL_SUCCESS_WITH_INFO) {
		generate_errstr("Error allocating connection", SQL_HANDLE_ENV, environment);
		return FALSE;
	}
	if (db_conntimeout > 0) {
		SQLSetConnectAttr(connection, SQL_LOGIN_TIMEOUT, (SQLPOINTER)(SQLLEN)db_conntimeout, 0);	//TODO: Ser ud til ikke at have noget effekt
		if (status != SQL_SUCCESS && status != SQL_SUCCESS_WITH_INFO) {
			generate_errstr("Error setting login timeout", SQL_HANDLE_DBC, connection);
			return FALSE;
		}
	}
	status = SQLDriverConnect(connection, NULL, (SQLCHAR*)connstr, strlen(connstr), NULL, 0, NULL, SQL_DRIVER_NOPROMPT);
	if (status != SQL_SUCCESS && status != SQL_SUCCESS_WITH_INFO) {
		generate_errstr("Error connecting to database", SQL_HANDLE_DBC, connection);
		return FALSE;
	}
	connsuccess = TRUE;
	return TRUE;
}

char db_isconnected() {
	if (!connect_conditions_ok()) return FALSE;
	SQLUINTEGER	n = 0;
	SQLRETURN status = SQLGetConnectAttr(connection, SQL_ATTR_CONNECTION_DEAD, &n, sizeof n, NULL);
	return (status == SQL_SUCCESS || status == SQL_SUCCESS_WITH_INFO) &&  n == SQL_CD_FALSE;
}

// ------------------------------ WRITE ------------------------------

char db_sqlwrite(char *sqlstr, int *affrows) {
	if (!connect_conditions_ok()) {
		generate_errstr("Not connected", 0, 0);
		return FALSE;
	}
	SQLHSTMT statement;
	SQLRETURN status = SQLAllocHandle(SQL_HANDLE_STMT, connection, &statement);
	if (status != SQL_SUCCESS && status != SQL_SUCCESS_WITH_INFO) {
		generate_errstr("Error allocating write statement", SQL_HANDLE_DBC, connection);
		return FALSE;
	}
	char retval = TRUE;
	status = SQLExecDirect(statement, (SQLCHAR*)sqlstr, strlen(sqlstr));
	if (status == SQL_SUCCESS || status == SQL_SUCCESS_WITH_INFO) {
		if (affrows) {
			SQLLEN n = -1;
			status = SQLRowCount(statement, &n);
			if (status == SQL_SUCCESS || status == SQL_SUCCESS_WITH_INFO) {
				*affrows = n;
			} else {
				retval = FALSE;
				generate_errstr("Error getting affected row count", SQL_HANDLE_STMT, statement);
			}
		}
	} else {
		retval = FALSE;
		generate_errstr("Error executing write statement", SQL_HANDLE_STMT, statement);
	}
	SQLFreeHandle(SQL_HANDLE_STMT, statement);
	return retval;
}

// ------------------------------ READ ------------------------------

SQLHSTMT read_statement = NULL;

char db_sqlread(char *sqlstr, DB_CALLBACK(cb_start), DB_CALLBACK(cb_row)) {
	if (!connect_conditions_ok()) {
		generate_errstr("Not connected", 0, 0);
		return FALSE;
	}
	SQLRETURN status = SQLAllocHandle(SQL_HANDLE_STMT, connection, &read_statement);
	if (!read_statement || (status != SQL_SUCCESS && status != SQL_SUCCESS_WITH_INFO)) {
		generate_errstr("Error allocating read statement", SQL_HANDLE_DBC, connection);
		return FALSE;
	}
	char retval = TRUE;
	status = SQLExecDirect(read_statement, (SQLCHAR*)sqlstr, strlen(sqlstr));
	if (status == SQL_SUCCESS || status == SQL_SUCCESS_WITH_INFO) {
		if (!cb_start || cb_start()) {
			for (;;) {
				status = SQLFetch(read_statement);
				if (status == SQL_NO_DATA) break;
				if (status != SQL_SUCCESS && status != SQL_SUCCESS_WITH_INFO) {
					retval = FALSE;
					generate_errstr("Error fetching row", SQL_HANDLE_STMT, read_statement);
					break;
				}
				if (cb_row && !cb_row()) break;
			}
		}
	} else {
		retval = FALSE;
		generate_errstr("Error executing read statement", SQL_HANDLE_STMT, read_statement);
	}
	SQLCloseCursor(read_statement);
	SQLFreeHandle(SQL_HANDLE_STMT, read_statement);
	read_statement = NULL;
	return retval;
}

long db_sqlread_rowcount() {
	SQLLEN n = -1;
	SQLRETURN status = SQLRowCount(read_statement, &n);
	if (status != SQL_SUCCESS && status != SQL_SUCCESS_WITH_INFO) {
		n = -1;
		generate_errstr("Error getting row count", SQL_HANDLE_STMT, read_statement);
	}
	return n;
}

short db_sqlread_colcount() {
	SQLSMALLINT n = -1;
	SQLRETURN status = SQLNumResultCols(read_statement, &n);
	if (status != SQL_SUCCESS && status != SQL_SUCCESS_WITH_INFO) {
		n = -1;
		generate_errstr("Error getting column count", SQL_HANDLE_STMT, read_statement);
	}
	return n;
}

char db_sqlread_coltype(short colno) {
	SQLSMALLINT datatype = 0;
	SQLRETURN status = SQLDescribeCol(read_statement, colno, NULL, 0, NULL, &datatype, NULL, NULL, NULL);
	if (status != SQL_SUCCESS && status != SQL_SUCCESS_WITH_INFO) {
		generate_errstr("Error getting column description", SQL_HANDLE_STMT, read_statement);
		return 0;
	}
	switch (datatype) {
		case SQL_BIT:
		case SQL_TINYINT:
		case SQL_SMALLINT:
		case SQL_INTEGER:	// _INTEGER
		case SQL_BIGINT:	// _BIGINT
			return 'i';
		case SQL_DECIMAL:	//TODO: korrekt?
		case SQL_NUMERIC:	//TODO: korrekt?
		case SQL_REAL:
		case SQL_FLOAT:
		case SQL_DOUBLE:
			return 'f';
		case SQL_CHAR:
		case SQL_VARCHAR:
		case SQL_LONGVARCHAR:
		case SQL_WCHAR:
		case SQL_WVARCHAR:
		case SQL_WLONGVARCHAR:
			return 's';
		case SQL_TYPE_DATE:
		case SQL_TYPE_TIME:
		case SQL_TYPE_TIMESTAMP:
			return 't';
	}
	return 0;
}

long db_sqlread_i(short colno) {
	long value = 0;
	SQLLEN ind;
	SQLRETURN status = SQLGetData(read_statement, colno, SQL_C_SLONG, &value, 0, &ind);
	if (status != SQL_SUCCESS && status != SQL_SUCCESS_WITH_INFO) {
		generate_errstr("Error getting integer value", SQL_HANDLE_STMT, read_statement);
		return 0;
	}
	if (ind == SQL_NULL_DATA) {
		generate_errstr("NULL", 0, 0);
		return 0;
	}
	return value;
}

extern double db_sqlread_f(short colno) {
	double value = 0.0;
	SQLLEN ind;
	SQLRETURN status = SQLGetData(read_statement, colno, SQL_C_DOUBLE, &value, 0, &ind);
	if (status != SQL_SUCCESS && status != SQL_SUCCESS_WITH_INFO) {
		generate_errstr("Error getting float value", SQL_HANDLE_STMT, read_statement);
		return 0;
	}
	if (ind == SQL_NULL_DATA) {
		generate_errstr("NULL", 0, 0);
		return 0;
	}
	return value;
}

char stringbuffer[256];

char* db_sqlread_s(short colno) {
	SQLLEN ind;
	stringbuffer[0] = 0;
	SQLRETURN status = SQLGetData(read_statement, colno, SQL_C_CHAR, &stringbuffer, sizeof stringbuffer, &ind);
	if (status != SQL_SUCCESS && status != SQL_SUCCESS_WITH_INFO) {
		generate_errstr("Error getting string value", SQL_HANDLE_STMT, read_statement);
		stringbuffer[0] = 0;
		return stringbuffer;
	}
	if (ind == SQL_NULL_DATA) {
		generate_errstr("NULL", 0, 0);
		stringbuffer[0] = 0;
		return stringbuffer;
	}
	stringbuffer[(sizeof stringbuffer)-1] = 0;
	return stringbuffer;
}

time_t db_sqlread_t(short colno) {
	SQL_TIMESTAMP_STRUCT value = {0, 0, 0, 0, 0, 0, 0};
	SQLLEN ind;
	SQLRETURN status = SQLGetData(read_statement, colno, SQL_C_TYPE_TIMESTAMP, &value, 0, &ind);
	if (status != SQL_SUCCESS && status != SQL_SUCCESS_WITH_INFO) {
		generate_errstr("Error getting date/time value", SQL_HANDLE_STMT, read_statement);
		return -1;
	}
	if (ind == SQL_NULL_DATA) {
		generate_errstr("NULL", 0, 0);
		return -1;
	}
	struct tm dt;
	dt.tm_sec = value.second;
	dt.tm_min = value.minute;
	dt.tm_hour = value.hour;
	dt.tm_mday = value.day;
	dt.tm_mon = value.month - 1;
	dt.tm_year = value.year - 1900;
	dt.tm_wday = 0;
	dt.tm_yday = 0;
	dt.tm_isdst = -1;	//TODO: -1 så passer ctime, mens 0 betyder "ikke sommertid" ???
	return mktime(&dt);
}
