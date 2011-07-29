/************************** MOO SQL v 1.0 ****************************************
 * copyright 2008, michael munson. this program is released under the BSD license *
 */
#include "moosql.h"

/* this function takes the integer given by do_mysql_connect() and returns a MYSQL_CONN associated with that */
MYSQL_CONN* resolve_mysql_connection(int id)
{
  int lcv=0;
  MYSQL_CONN *ptr=NULL;
  for (lcv=0;lcv<MOOSQL_MAX_CON;lcv++)
    {
      if (SQL_CONNECTIONS[lcv].id == id)
	return &SQL_CONNECTIONS[lcv];
    }
  return (NULL);
}

void sanitize_result_string(char *string)
{
  /* I just learned the hard way that it is a really bad idea to have newlines in your LambdaMOO database file */
  /* We'll convert each newline to a tab character instead, which is relatively harmless but still identifiable */
  /* see moosql.h to undefine SANITIZE_STRINGS if your version of LambdaMOO can handle newlines */
#ifdef SANITIZE_STRINGS

  int lcv=0;
  char *ptr=string;
  for (lcv=0;lcv<strlen(string);lcv++)
    {
      if (*ptr == '\n')
	*ptr = '\t';
      ptr++; /* go to next character */
    }

#endif
}

int connection_array_index()
{
  /* This function returns the index for the first blank entry in SQL_CONNECTIONS. Returns -1 if none are available */
  int lcv=0;
  for (lcv=0; lcv < MOOSQL_MAX_CON; lcv++)
    {
      if (!SQL_CONNECTIONS[lcv].active)
	return lcv;
      else if (SQL_CONNECTIONS[lcv].conn == NULL)
	{
	  /* Somehow the connection is null but the active flag hasn't been reset. Oh well, we'll do that. */
	  SQL_CONNECTIONS[lcv].active = 0;
	  SQL_CONNECTIONS[lcv].id = 0;
	  SQL_CONNECTIONS[lcv].port = 0;
	  return lcv;
	}
    }
  return -1;
}

MYSQL_CONN *do_mysql_connect (const char *host_name, const char *user_name, const char *password, const char *db_name,
      const unsigned int port_num, const char *socket_name, const unsigned int flags, char *error_string)
{
MYSQL  *conn; /* pointer to connection handler */

 int ndx=0;
 ndx = connection_array_index(); /* find out where it should go. */
 if (ndx == -1) /* No room in the index! */
   {
     snprintf(error_string, MOOSQL_ERROR_LEN, "no mysql connections available, close some?");
   return (NULL);
   }

 conn = mysql_init (NULL);  /* allocate the handler */
  if (conn == NULL)
  {
    snprintf (error_string, MOOSQL_ERROR_LEN, "mysql_init failed");
    return (NULL);
  }
  if (mysql_real_connect (conn, host_name, user_name, password,
            db_name, port_num, socket_name, flags) == NULL)
  {
    snprintf (error_string, MOOSQL_ERROR_LEN, "Error %u (%s)",
              mysql_errno (conn), mysql_error (conn));
    return (NULL);
  }
  SQL_CONNECTIONS[ndx].active = 1;
  SQL_CONNECTIONS[ndx].id = next_mysql_connection--;
  SQL_CONNECTIONS[ndx].port = port_num;
  SQL_CONNECTIONS[ndx].connect_time = time(0);
  SQL_CONNECTIONS[ndx].last_query_time = time(0);
  snprintf(SQL_CONNECTIONS[ndx].server,MOOSQL_STRING_LEN,host_name, "%s");
  snprintf(SQL_CONNECTIONS[ndx].username,MOOSQL_STRING_LEN,user_name, "%s");
  snprintf(SQL_CONNECTIONS[ndx].database,MOOSQL_STRING_LEN,db_name, "%s");
  SQL_CONNECTIONS[ndx].conn = conn;

  return &SQL_CONNECTIONS[ndx];
}


void do_mysql_disconnect (MYSQL_CONN *conn_wrapper)
{ 
  conn_wrapper->active = 0;
  conn_wrapper->id = 0;
  conn_wrapper->port = 0;
  conn_wrapper->connect_time = 0;
  conn_wrapper->last_query_time = 0;
  mysql_close (conn_wrapper->conn);
  conn_wrapper->conn = NULL;
 /* we could zero up the strings on the wrapper if we wanted to here */
}


Var process_row(MYSQL_RES *res_set, const MYSQL_ROW *row)
{
  Var r;
  int len=0;
  int i=0;
  double dval=0.0;
  len = mysql_num_fields(res_set);
  r.type = TYPE_LIST;
  r = new_list(len);
  for (i=0;i<len;i++)
    {
      r.v.list[i+1].type = TYPE_STR;
      if (row[i] == NULL)
	r.v.list[i+1].v.str = str_dup("");
      else
	{
	  sanitize_result_string((char*)row[i]);
	  r.v.list[i+1].v.str = str_dup((char*)row[i]);
	}
    }
  return r;
}

Var process_result_set (MYSQL_CONN *conn, MYSQL_RES *res_set)
{
MYSQL_ROW    row;
unsigned int  i;
 int len = 0;
 int count = 1;
 int lcv = 1;
 len = mysql_num_rows(res_set);
 Var r;
 r.type = TYPE_LIST;
 if (len < 1)
   {
     r = new_list(0);
     mysql_free_result (res_set);
     return r;
   }
     
 r = new_list(len);
 while ((row = mysql_fetch_row (res_set)) != NULL)
    {
      r.v.list[count].type = TYPE_LIST;
      r.v.list[count].v.list = process_row(res_set,(const MYSQL_ROW*)row).v.list;
      count++;
    }
 mysql_free_result (res_set);
 return r;
}


#if !defined(MYSQL_VERSION_ID) || MYSQL_VERSION_ID<32224
#define mysql_field_count mysql_num_fields
#endif

MYSQL_RES* process_mysql_query (MYSQL_CONN *conn, char * res_string)
{
MYSQL_RES *res_set;
unsigned int field_count;

 /* query succeeded err */
 res_set = mysql_store_result (conn->conn);
 if (res_set == NULL) /* no result */
   {
     /* check if its an error or just a query with no result */
     if (mysql_field_count (conn->conn) > 0)
       {
	 snprintf(res_string,MOOSQL_ERROR_LEN,"Error processing SQL result set");
	 return NULL;
       }
     else
       {
	 snprintf (res_string,MOOSQL_ERROR_LEN, "%lu rows affected.",(unsigned long) mysql_affected_rows (conn->conn));
       return NULL;
       }
  }
  else
  {
    return res_set;
  }
}


/* our MOO functions are below */

int mysql_connection_status(MYSQL_CONN *wrapper)
{
	int ping;
	
	if (wrapper == NULL || wrapper->conn == NULL)
	  return 0;
	ping = mysql_ping(wrapper->conn);
	if (ping == 0)
	  return 1;
	else
	  return 0;
}

static package bf_mysql_ping(Var arglist, Byte next, void *vdata, Objid progr)
{
	/* we will return 1 if the connection is active, 0 if it is not */
	Var r;
	Objid oid = arglist.v.list[1].v.obj;
	free_var(arglist);
	if (!is_wizard(progr))
	  return make_error_pack(E_PERM);
	MYSQL_CONN *wrapper;
	wrapper = resolve_mysql_connection(oid);
	r.type = TYPE_INT;
	r.v.num = mysql_connection_status(wrapper);
	return make_var_pack(r);	
}

static package bf_mysql_status(Var arglist, Byte next, void *vdata, Objid progr)
{
	Var r;
	Objid oid = arglist.v.list[1].v.obj;
	free_var(arglist);
	if (!is_wizard(progr))
	  return make_error_pack(E_PERM);
	MYSQL_CONN *wrapper;
	wrapper = resolve_mysql_connection(oid);
	if (wrapper == NULL || wrapper->active == 0)
	  return make_error_pack(E_INVARG);
	r = new_list(7);
	r.v.list[1].type = TYPE_STR;
	r.v.list[1].v.str = str_dup(wrapper->server);
	r.v.list[2].type = TYPE_INT;
	r.v.list[2].v.num = wrapper->port;
	r.v.list[3].type = TYPE_STR;
	r.v.list[3].v.str = str_dup(wrapper->username);
	r.v.list[4].type = TYPE_STR;
	r.v.list[4].v.str = str_dup(wrapper->database);
	r.v.list[5].type = TYPE_INT;
	r.v.list[5].v.num = wrapper->connect_time;
	r.v.list[6].type = TYPE_INT;
	r.v.list[6].v.num = wrapper->last_query_time;
	r.v.list[7].type = TYPE_INT;
	r.v.list[7].v.num = mysql_get_server_version(wrapper->conn);
	return make_var_pack(r);
}

static package bf_mysql_connections(Var arglist, Byte next, void *vdata, Objid progr)
{
  Var r;
  free_var(arglist);
  if (!is_wizard(progr))
    return make_error_pack(E_PERM);
  int count=0;
  int lcv=0;
  for (lcv=0;lcv<MOOSQL_MAX_CON;lcv++)
    if (SQL_CONNECTIONS[lcv].active == 1)
      count++;
  r = new_list(count); /* needed to know how many to allocate the list firstly */
  count = 0;
  for (lcv=0;lcv<MOOSQL_MAX_CON;lcv++)
    {
      if (SQL_CONNECTIONS[lcv].active == 1)
	{
	  count++;
	  r.v.list[count].type = TYPE_OBJ;
	  r.v.list[count].v.obj = SQL_CONNECTIONS[lcv].id;
	}
    }
  return make_var_pack(r);
}

static package bf_mysql_close(Var arglist, Byte next, void *vdata, Objid progr)
{
	Var r;
	Objid oid = arglist.v.list[1].v.obj;
	r.type = TYPE_INT;
	free_var(arglist);
	MYSQL_CONN *wrapper;
	if (!is_wizard(progr))
		return make_error_pack(E_PERM);
	wrapper = resolve_mysql_connection(oid);
	if (wrapper == NULL)
	  {
	    r.v.num = 0;
	    return make_var_pack(r);
	  }
	else 
	  {
	    wrapper->active = 0;
	    wrapper->id = 0;
	    wrapper->port = 0;
	    wrapper->connect_time = 0;
	    wrapper->last_query_time = 0;
	    r.v.num = 1;
	  }
	if (mysql_connection_status(wrapper) != 0)
	{
		do_mysql_disconnect(wrapper);
		wrapper->conn = NULL;
		r.v.num = 1;
	}
	return make_var_pack(r);
}

static package bf_mysql_escape( Var arglist, Byte next, void *vdata, Objid progr)
{
  Var r;
  MYSQL_CONN *wrapper;
  if (!is_wizard(progr))
  {
    free_var(arglist);
    return make_error_pack(E_PERM);
  }
  Objid oid = arglist.v.list[1].v.obj;
  wrapper = resolve_mysql_connection(oid);
  if (wrapper == NULL || wrapper->conn == NULL || wrapper->active == 0)
  {
    free_var(arglist);
    return make_error_pack(E_INVARG);
  }
  const char *string = arglist.v.list[2].v.str;
  free_var(arglist);
  ulong bytes = strlen(string);
  char ostring[(2 * bytes)+1];
  mysql_real_escape_string(wrapper->conn,ostring,string,bytes);
  r.type = TYPE_STR;
  r.v.str = str_dup(ostring);
  return make_var_pack(r);
}

static package bf_mysql_query( Var arglist, Byte next, void *vdata, Objid progr)
{
  Var r;
  char error_string[MOOSQL_ERROR_LEN];
  MYSQL_CONN *wrapper;
  MYSQL_RES *res_set;
#ifdef MOOSQL_MULTIPLE_STATEMENTS
  Var tmp;
  Var end;
  int len = 0; 
  int continu = 1;
#endif
  if (!is_wizard(progr))
    {
      free_var(arglist);
      return make_error_pack(E_PERM);
    }
  Objid oid = arglist.v.list[1].v.obj;
  wrapper = resolve_mysql_connection(oid);
  if (wrapper == NULL || wrapper->conn == NULL || wrapper->active == 0)
    {
      free_var(arglist);
      return make_error_pack(E_INVARG);
    }
  const char *query = arglist.v.list[2].v.str;
  free_var(arglist);
  /* we do the query now. */
  if (mysql_query (wrapper->conn, query) != 0)   /* failed */
   {
     /* there is an error, so we will return that string. similar to below which
      * returns a string for a successful query with no result set which is handled in
      * process_mysql_query */
     snprintf(error_string,MOOSQL_ERROR_LEN,"ERR: %s",mysql_error(wrapper->conn));
     r.type = TYPE_STR;
     r.v.str = str_dup(error_string);
     return make_var_pack(r);
   }
  wrapper->last_query_time = time(0);
#ifdef MOOSQL_MULTIPLE_STATEMENTS
  r = new_list(1);
  r.v.list[1].type = TYPE_INT;
  r.v.list[1].v.num = 0;
  end = new_list(0);
  while (continu)
    {
      len++;
      res_set = process_mysql_query(wrapper,error_string);
      if (res_set == NULL) /* there was no result on this query */
	{
	  tmp.type = TYPE_STR;
	  tmp.v.str = str_dup(error_string);
	  end = listappend(end,var_dup(tmp));
	}
      else 
	{
	  tmp = process_result_set(wrapper,res_set);
	  end = listappend(end,var_dup(tmp));
	}
      if (mysql_more_results(wrapper->conn))
	{
	  mysql_next_result(wrapper->conn);
	  continu = 1;
	}
      else
	continu = 0;
    }
  if (len <= 1) /* if there is only one result return it like in previous versions, a list of rows */
    return make_var_pack(end.v.list[1]);
  r.v.list[1].v.num = len;
  /* if there are more return it in this format {X, Y} where X is the number of results and Y is a list of results */
  r = listappend(r,end); 
  return make_var_pack(r);
#else
  res_set = process_mysql_query(wrapper,error_string);

  if (res_set == NULL) /* there was either an error / no result on this query */
    {
      r.type = TYPE_STR;
      r.v.str = str_dup(error_string);
      return make_var_pack(r);
    }
  r = process_result_set(wrapper,res_set); 
  return make_var_pack(r);
#endif

}

static package
bf_mysql_connect(Var arglist, Byte next, void *vdata, Objid progr)
{

#ifdef OUTBOUND_NETWORK
	Var r;
	char error_string[MOOSQL_ERROR_LEN];
	MYSQL_CONN *wrapper;
	if (!is_wizard(progr)) 
	{
		free_var(arglist);
		return make_error_pack(E_PERM);
	}

	const char *hostname = arglist.v.list[1].v.str;
	const int port = arglist.v.list[2].v.num;
	const char *username = arglist.v.list[3].v.str;
	const char *password = arglist.v.list[4].v.str;
	const char *dbname = arglist.v.list[5].v.str;
	free_var(arglist); /* get rid of that now */
	/* try to connect to mysql server */
	/* check if we have enough connection slots. */
	int index=-1;
	index = connection_array_index();
	if (index == -1)
	  return make_error_pack(E_QUOTA);
#ifdef MOOSQL_MULTIPLE_STATEMENTS
	wrapper = do_mysql_connect(hostname, username, password, dbname, port, NULL, CLIENT_MULTI_STATEMENTS, error_string);
#else
	wrapper = do_mysql_connect(hostname, username, password, dbname, port, NULL, 0, error_string);
#endif
	if (wrapper == NULL || wrapper->conn == NULL)
	{
		/* an error happened in the connect, return that as a STR */
		r.type = TYPE_STR;
		r.v.str = str_dup(error_string);
		return make_var_pack(r);
	}
	r.type = TYPE_OBJ;
	r.v.obj = wrapper->id;
	return make_var_pack(r);
	
	
#else                           /* !OUTBOUND_NETWORK */

	    /* This function is disabled in this server. */
	free_var(arglist);
	return make_error_pack(E_PERM);
		
#endif
}

void register_mysql(void)
{
	(void) register_function("mysql_connect", 5, 5, bf_mysql_connect, TYPE_STR, TYPE_INT, TYPE_STR, TYPE_STR, TYPE_STR);
	(void) register_function("mysql_close", 1, 1, bf_mysql_close, TYPE_OBJ);
	(void) register_function("mysql_status",1,1,bf_mysql_status,TYPE_OBJ);
	(void) register_function("mysql_ping",1,1,bf_mysql_ping,TYPE_OBJ);
	(void) register_function("mysql_query",2,2,bf_mysql_query,TYPE_OBJ,TYPE_STR);
        (void) register_function("mysql_connections",0,0,bf_mysql_connections);
        (void) register_function("mysql_escape",2,2,bf_mysql_escape,TYPE_OBJ,TYPE_STR);
}
