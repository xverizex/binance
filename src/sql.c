#include <sqlite3.h>
#include <string.h>
#include <stdio.h>
#include "sql.h"
#include <time.h>

extern char *config_file_error;

static void sql_error ( const char *error ) {
	FILE *fp = fopen ( config_file_error, "a" );
	time_t cur_time = time ( NULL );
	fprintf ( fp, "%s %s\n", ctime ( &cur_time ), error );
	fclose ( fp );
}

sqlite3 *sql;
sqlite3_stmt *stmt;

int sql_open ( const char *sql_file ) {
	return sqlite3_open ( sql_file, &sql );
}


void sql_create_table_if_not_exists ( ) {
	const char *query = "CREATE TABLE IF NOT EXISTS curs ( id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL, date STRING, currency STRING, low DOUBLE, high DOUBLE );";
	int ret = sqlite3_prepare_v2 ( sql, query, strlen ( query ), &stmt, NULL );
	if ( ret == -1 ) {
		sql_error ( "Ошибка в подготовке таблицы curs." );
	}
	ret = sqlite3_step ( stmt );
}

void sql_put_records ( const char *query ) {
	int ret = sqlite3_prepare_v2 ( sql, query, strlen ( query ), &stmt, NULL );
	if ( ret == -1 ) {
		sql_error ( "Ошибка в записи данных в таблицу." );
		return;
	}
	ret = sqlite3_step ( stmt );
}

void sql_get_info ( const char *date_time, const char *currency ) {
	char query[255];
	if ( date_time == NULL && currency == NULL ) {
		snprintf ( query, 255, "SELECT id, date, currency, low, high FROM curs;" );
	} else {
		snprintf ( query, 255, "SELECT date, currency, low, high FROM curs WHERE date = '%s' AND currency = '%s';", date_time, currency );
	}
	sqlite3_prepare_v2 ( sql, query, strlen ( query ), &stmt, NULL );
}
void sql_get_info_btc ( ) {
	char query[255];
	snprintf ( query, 255, "SELECT date, low, high FROM curs WHERE currency = '%s';", "BTCUSDT" );
	sqlite3_prepare_v2 ( sql, query, strlen ( query ), &stmt, NULL );
}

void sql_debug_get_info ( ) {
	char query[255];
	snprintf ( query, 255, "SELECT id, date, currency, low, high FROM curs;" );
	int ret = sqlite3_prepare_v2 ( sql, query, strlen ( query ), &stmt, NULL );
	if ( ret == SQLITE_OK ) {
		printf ( "ok\n" );
	}
	if ( sqlite3_step ( stmt ) == SQLITE_ROW ) {
		printf ( "%s\n", "есть запись" );
	}

}

int sql_get_step ( ) {
	if ( sqlite3_step ( stmt ) == SQLITE_ROW ) return SQL_ROW;
	return SQL_ERROR;
}

const char *sql_get_string ( const int index ) {
	return sqlite3_column_text ( stmt, index );
}

double sql_get_double ( const int index ) {
	return sqlite3_column_double ( stmt, index );
}
int sql_get_int ( const int index ) {
	return sqlite3_column_int ( stmt, index );
}
