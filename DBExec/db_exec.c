#include <stdio.h>
#include <string.h>
#include "../database.h"

int column_count = 0;

int cb_start() {
	printf("Found approximately %ld rows:\n", db_sqlread_rowcount());
	column_count = db_sqlread_colcount();
	return 1;
}

int cb_row() {
	char *s;
	for (int i = 1; i <= column_count; i++) {
		if (i > 1) printf("\t");
		db_errorstr[0] = 0;
		s = db_sqlread_s(i);
		if (db_errorstr[0] == 0)
			printf("%s", s);
		else
		if (strcmp(db_errorstr, "NULL") == 0)
			printf("NULL");
		else
			printf("%s", db_errorstr);
	}
	printf("\n");
	return 1;
}

int main(const int argc, const char *argv[]) {
	char write_sql;
	char connection_string[512];
	char sql_string[4096];

	// Command line arguments
	if (argc != 4) {
		printf("DB Exec usage:\n\n");
		printf("%s \"Connection-string\" READ|WRITE \"SQL-string\"\n", argv[0]);
		return 1;
	}
	if (strcmp(argv[2], "READ") == 0)
		write_sql = 0;
	else 
	if (strcmp(argv[2], "WRITE") == 0)
		write_sql = 1;
	else {
		printf("Use either READ or WRITE as second argument\n");
		return 2;
	}
	strncpy(connection_string, argv[1], (sizeof connection_string) - 1);
	strncpy(sql_string, argv[3], (sizeof sql_string) - 1);
	
	// Connect to database
	if (!db_connect(connection_string)) {
		printf("%s\n", db_errorstr);
		db_disconnect();
		return 3;
	}
	
	// Execute SQL
	if (write_sql) {
		// Updating SQL
		int affected_rows;
		if (db_sqlwrite(sql_string, &affected_rows)) {
			printf("Database was updated with %d row(s) affected\n", affected_rows);
		} else {
			printf("%s\n", db_errorstr);
			db_disconnect();
			return 4;
		}
	} else {
		// Selective SQL
		if (!db_sqlread(sql_string, cb_start, cb_row)) {
			printf("%s\n", db_errorstr);
			db_disconnect();
			return 5;
		}
	}
	db_disconnect();
	printf("Success\n");
	return 0;
}