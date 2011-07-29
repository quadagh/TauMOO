/************************* MOO SQL v1.0 *******************************************
 * copyright 2008 michael munson, this software is released under the BSD license *
 */
#include <stdio.h>
#include <mysql.h>
#include <ctype.h>
#include "my-time.h"
#include "my-string.h"
#include "config.h"
#include "functions.h"
#include "log.h"
#include "random.h"
#include "storage.h"
#include "utils.h"
#include "dirent.h"
#include "unparse.h"
#include "list.h"
#include "regexpr.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#include <errno.h>
#include "version.h"
#include <string.h>


#define MOOSQL_ERROR_LEN 512 /* maximum length of a MySQL error message */
#define MOOSQL_MAX_CON 5 /* maximum simultaneous MySQL connections */
#define MOOSQL_STRING_LEN 256
#define SANITIZE_STRINGS 1 /* IF THIS IS 0 SANITIZE_RESULT_STRING WONT BE CALLED */
// #define MOOSQL_MULTIPLE_STATEMENTS  /* comment this out if you dont want multiple line statements. */

typedef struct MYSQL_CONN
{
  int connect_time;
  int last_query_time;
  int active;
  int port;
  Objid id;
  char server[MOOSQL_STRING_LEN];
  char username[MOOSQL_STRING_LEN];
  char database[MOOSQL_STRING_LEN];
  MYSQL *conn;
} MYSQL_CONN;


MYSQL_CONN * do_mysql_connect (const char *host_name, const char *user_name, const char *password, const char *db_name,
				      const unsigned int port_num, const char *socket_name, const unsigned int flags, char *error_string);
void do_mysql_disconnect (MYSQL_CONN *conn);
int mysql_connection_ping (MYSQL_CONN *conn);
void sanitize_result_string(char *string);
int mysql_connection_status(MYSQL_CONN *wrapper);
Var process_row(MYSQL_RES *res_set, const MYSQL_ROW *row);
Var process_result_set (MYSQL_CONN *conn, MYSQL_RES *res_set);
/* ------- UGLY GLOBAL VARIABLES GO HERE ----- */
Objid next_mysql_connection = NOTHING - 1; /* instead of using unconnected_player we'll use our own since theres no
					      					    * nhandler attached to our version */
static MYSQL_CONN SQL_CONNECTIONS[MOOSQL_MAX_CON]; /* Our array of connections. */
/* ----- DONE WITH UGLY GLOBALS ---- */

int connection_array_index(); /* returns the index to the array for the first unused connection. 0 if none. */
MYSQL_CONN* resolve_mysql_connection(int); /* this verb takes the int id assigned by do_mysql_connect and returns the correct wrapper */
