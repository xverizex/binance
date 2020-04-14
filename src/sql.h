#ifndef __SQL_H__

#define SQL_ROW                 0
#define SQL_ERROR              -1
int sql_open ( const char *sql_file );
void sql_create_table_if_not_exists ( void );
void sql_put_records ( const char *query );
void sql_get_info ( const char *date_time, const char *currency );
int sql_get_step ( void );
const char *sql_get_string ( const int index );
double sql_get_double ( const int index );
int sql_get_int ( const int index );
void sql_debug_get_info ( void );
void sql_get_info_btc ( void );
void sql_get_info_eth ( void );
#endif
