/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2003-2008 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version two of the GNU General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bacula® is a registered trademark of John Walker.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/
/*
 * Bacula Catalog Database routines specific to DBI
 *   These are DBI specific routines
 *
 *    João Henrique Freitas, December 2007
 *    based upon work done by Dan Langille, December 2003 and
 *    by Kern Sibbald, March 2000
 *
 *    Version $Id$
 */


/* The following is necessary so that we do not include
 * the dummy external definition of DB.
 */
#define __SQL_C                       /* indicate that this is sql.c */

#include "bacula.h"
#include "cats.h"

#ifdef HAVE_DBI

/* -----------------------------------------------------------------------
 *
 *   DBI dependent defines and subroutines
 *
 * -----------------------------------------------------------------------
 */

/* List of open databases */
static BQUEUE db_list = {&db_list, &db_list};

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/*
 * Retrieve database type
 */
const char *
db_get_type(void)
{
   return "DBI";
}

/*
 * Initialize database data structure. In principal this should
 * never have errors, or it is really fatal.
 */
B_DB *
db_init_database(JCR *jcr, const char *db_name, const char *db_user, const char *db_password,
                 const char *db_address, int db_port, const char *db_socket, 
                 int mult_db_connections)
{
   B_DB *mdb;
   char db_driver[10];
   char db_driverdir[256];

   /* Constraint the db_driver */
   if(db_type  == -1) {
      Jmsg(jcr, M_FATAL, 0, _("A dbi driver for DBI must be supplied.\n"));
      return NULL;
   }
   
   /* Do the correct selection of driver. 
    * Can be one of the varius supported by libdbi 
    */
   switch (db_type) {
   case SQL_TYPE_MYSQL:
      bstrncpy(db_driver,"mysql", sizeof(db_driver));
      break;
   case SQL_TYPE_POSTGRESQL:
      bstrncpy(db_driver,"pgsql", sizeof(db_driver));
      break;
   case SQL_TYPE_SQLITE:
      bstrncpy(db_driver,"pgsql", sizeof(db_driver));
      break;
   }
   
   /* Set db_driverdir whereis is the libdbi drivers */
   bstrncpy(db_driverdir, DBI_DRIVER_DIR, 255);
   
   if (!db_user) {
      Jmsg(jcr, M_FATAL, 0, _("A user name for DBI must be supplied.\n"));
      return NULL;
   }
   P(mutex);                          /* lock DB queue */
   if (!mult_db_connections) {
      /* Look to see if DB already open */
      for (mdb=NULL; (mdb=(B_DB *)qnext(&db_list, &mdb->bq)); ) {
         if (bstrcmp(mdb->db_name, db_name) &&
             bstrcmp(mdb->db_address, db_address) &&
             bstrcmp(mdb->db_driver, db_driver) &&
             mdb->db_port == db_port) { 
            Dmsg3(100, "DB REopen %d %s %s\n", mdb->ref_count, db_driver, db_name);
            mdb->ref_count++;
            V(mutex);
            return mdb;                  /* already open */
         }
      }
   }
   Dmsg0(100, "db_open first time\n");
   mdb = (B_DB *)malloc(sizeof(B_DB));
   memset(mdb, 0, sizeof(B_DB));
   mdb->db_name = bstrdup(db_name);
   mdb->db_user = bstrdup(db_user);
   if (db_password) {
      mdb->db_password = bstrdup(db_password);
   }
   if (db_address) {
      mdb->db_address  = bstrdup(db_address);
   }
   if (db_socket) {
      mdb->db_socket   = bstrdup(db_socket);
   }
   if (db_driverdir) {
      mdb->db_driverdir = bstrdup(db_driverdir);
   }
   if (db_driver) {
      mdb->db_driver    = bstrdup(db_driver);
   }
   mdb->db_type        = db_type;
   mdb->db_port        = db_port;
   mdb->have_insert_id = TRUE;
   mdb->errmsg         = get_pool_memory(PM_EMSG); /* get error message buffer */
   *mdb->errmsg        = 0;
   mdb->cmd            = get_pool_memory(PM_EMSG); /* get command buffer */
   mdb->cached_path    = get_pool_memory(PM_FNAME);
   mdb->cached_path_id = 0;
   mdb->ref_count      = 1;
   mdb->fname          = get_pool_memory(PM_FNAME);
   mdb->path           = get_pool_memory(PM_FNAME);
   mdb->esc_name       = get_pool_memory(PM_FNAME);
   mdb->esc_path      = get_pool_memory(PM_FNAME);
   mdb->allow_transactions = mult_db_connections;
   qinsert(&db_list, &mdb->bq);            /* put db in list */
   V(mutex);
   return mdb;
}

/*
 * Now actually open the database.  This can generate errors,
 *   which are returned in the errmsg
 *
 * DO NOT close the database or free(mdb) here  !!!!
 */
int
db_open_database(JCR *jcr, B_DB *mdb)
{   
   int errstat;
   int dbstat;
   const char *errmsg;
   char buf[10], *port;
   int numdrivers; 
   
   P(mutex);
   if (mdb->connected) {
      V(mutex);
      return 1;
   }
   mdb->connected = false;

   if ((errstat=rwl_init(&mdb->lock)) != 0) {
      berrno be;
      Mmsg1(&mdb->errmsg, _("Unable to initialize DB lock. ERR=%s\n"),
            be.bstrerror(errstat));
      V(mutex);
      return 0;
   }

   if (mdb->db_port) {
      bsnprintf(buf, sizeof(buf), "%d", mdb->db_port);
      port = buf;
   } else {
      port = NULL;
   }
   
   numdrivers = dbi_initialize(mdb->db_driverdir);
   if (numdrivers < 0) {
      dbi_shutdown();
      Mmsg2(&mdb->errmsg, _("Unable to locate the DBD drivers to DBI interface in: \n"
                               "db_driverdir=%s. It is probaly not found any drivers\n"),
                               mdb->db_driverdir,numdrivers);
      V(mutex);
      return 0;
   }
   mdb->db = (void **)dbi_conn_new(mdb->db_driver);
   dbi_conn_set_option(mdb->db, "host", mdb->db_address); /* default = localhost */
   dbi_conn_set_option(mdb->db, "port", port);            /* default port */
   dbi_conn_set_option(mdb->db, "username", mdb->db_user);     /* login name */
   dbi_conn_set_option(mdb->db, "password", mdb->db_password); /* password */
   dbi_conn_set_option(mdb->db, "dbname", mdb->db_name);       /* database name */
      
   /* If connection fails, try at 5 sec intervals for 30 seconds. */
   for (int retry=0; retry < 6; retry++) {
         
      dbstat = dbi_conn_connect(mdb->db); 
      if ( dbstat == 0) {
         break;
      }
         
      dbi_conn_error(mdb->db, &errmsg);
      Dmsg1(50, "dbi error: %s\n", errmsg);
                   
      bmicrosleep(5, 0);
         
   }
   
   if ( dbstat != 0 ) {
      Mmsg3(&mdb->errmsg, _("Unable to connect to DBI interface.\n"
                       "Type=%s Database=%s User=%s\n"
                       "It is probably not running or your password is incorrect.\n"),
                        mdb->db_driver, mdb->db_name, mdb->db_user);
      V(mutex);
      return 0;           
   } 
   
   Dmsg0(50, "dbi_real_connect done\n");
   Dmsg3(50, "db_user=%s db_name=%s db_password=%s\n",
                    mdb->db_user, mdb->db_name,
                    mdb->db_password==NULL?"(NULL)":mdb->db_password);

   mdb->connected = true;

   if (!check_tables_version(jcr, mdb)) {
      V(mutex);
      return 0;
   }
   
   switch (mdb->db_type) {
   case SQL_TYPE_MYSQL:
      /* Set connection timeout to 8 days specialy for batch mode */
      sql_query(mdb, "SET wait_timeout=691200");
      sql_query(mdb, "SET interactive_timeout=691200");
      break;
   case SQL_TYPE_POSTGRESQL:
      /* tell PostgreSQL we are using standard conforming strings
         and avoid warnings such as:
         WARNING:  nonstandard use of \\ in a string literal
      */
      sql_query(mdb, "SET datestyle TO 'ISO, YMD'");
      sql_query(mdb, "set standard_conforming_strings=on");
      break;
   case SQL_TYPE_SQLITE:
      break;
   }
   
   V(mutex);
   return 1;
}

void
db_close_database(JCR *jcr, B_DB *mdb)
{
   if (!mdb) {
      return;
   }
   db_end_transaction(jcr, mdb);
   P(mutex);
   sql_free_result(mdb);
   mdb->ref_count--;
   if (mdb->ref_count == 0) {
      qdchain(&mdb->bq);
      if (mdb->connected && mdb->db) {
         sql_close(mdb);
         mdb->db = NULL;
      }
      rwl_destroy(&mdb->lock);
      free_pool_memory(mdb->errmsg);
      free_pool_memory(mdb->cmd);
      free_pool_memory(mdb->cached_path);
      free_pool_memory(mdb->fname);
      free_pool_memory(mdb->path);
      free_pool_memory(mdb->esc_name);
      free_pool_memory(mdb->esc_path);
      if (mdb->db_name) {
         free(mdb->db_name);
      }
      if (mdb->db_user) {
         free(mdb->db_user);
      }
      if (mdb->db_password) {
         free(mdb->db_password);
      }
      if (mdb->db_address) {
         free(mdb->db_address);
      }
      if (mdb->db_socket) {
         free(mdb->db_socket);
      }
      dbi_shutdown();
      if (mdb->db_driver) {
          free(mdb->db_driver);
      }
      free(mdb);
      
      
   }   
   V(mutex);
}

void db_thread_cleanup()
{ }

/*
 * Return the next unique index (auto-increment) for
 * the given table.  Return NULL on error.
 *
 */
int db_next_index(JCR *jcr, B_DB *mdb, char *table, char *index)
{
   strcpy(index, "NULL");
   return 1;
}


/*
 * Escape strings so that DBI is happy
 *
 *   NOTE! len is the length of the old string. Your new
 *         string must be long enough (max 2*old+1) to hold
 *         the escaped output.
 * 
 * dbi_conn_quote_string_copy receives a pointer to pointer.
 * We need copy the value of pointer to snew. Because libdbi change the 
 * pointer
 */
void
db_escape_string(JCR *jcr, B_DB *mdb, char *snew, char *old, int len)
{
   char *inew;
   char *pnew;
   
   if (len == 0) {
          snew[0] = 0; 
   } else {
          /* correct the size of old basead in len and copy new string to inew */
          inew = (char *)malloc(sizeof(char) * len + 1);
          bstrncpy(inew,old,len + 1);
          /* escape the correct size of old */
          dbi_conn_escape_string_copy(mdb->db, inew, &pnew);
          /* copy the escaped string to snew */
      bstrncpy(snew, pnew, 2 * len + 1);   
   }
   
   Dmsg2(500, "dbi_conn_escape_string_copy %p %s\n",snew,snew);
   
}

/*
 * Submit a general SQL command (cmd), and for each row returned,
 *  the sqlite_handler is called with the ctx.
 */
bool db_sql_query(B_DB *mdb, const char *query, DB_RESULT_HANDLER *result_handler, void *ctx)
{
   SQL_ROW row;

   Dmsg0(500, "db_sql_query started\n");

   db_lock(mdb);
   if (sql_query(mdb, query) != 0) {
      Mmsg(mdb->errmsg, _("Query failed: %s: ERR=%s\n"), query, sql_strerror(mdb));
      db_unlock(mdb);
      Dmsg0(500, "db_sql_query failed\n");
      return false;
   }
   Dmsg0(500, "db_sql_query succeeded. checking handler\n");

   if (result_handler != NULL) {
      Dmsg0(500, "db_sql_query invoking handler\n");
      if ((mdb->result = sql_store_result(mdb)) != NULL) {
         int num_fields = sql_num_fields(mdb);

         Dmsg0(500, "db_sql_query sql_store_result suceeded\n");
         while ((row = sql_fetch_row(mdb)) != NULL) {

            Dmsg0(500, "db_sql_query sql_fetch_row worked\n");
            if (result_handler(ctx, num_fields, row))
               break;
         }

        sql_free_result(mdb);
      }
   }
   db_unlock(mdb);

   Dmsg0(500, "db_sql_query finished\n");

   return true;
}



DBI_ROW my_dbi_fetch_row(B_DB *mdb)
{
   int j;
   DBI_ROW row = NULL; // by default, return NULL

   Dmsg0(500, "my_dbi_fetch_row start\n");

   if (!mdb->row || mdb->row_size < mdb->num_fields) {
      int num_fields = mdb->num_fields;
      Dmsg1(500, "we have need space of %d bytes\n", sizeof(char *) * mdb->num_fields);

      if (mdb->row) {
         Dmsg0(500, "my_dbi_fetch_row freeing space\n");
         free(mdb->row);
      }
      num_fields += 20;                  /* add a bit extra */
      mdb->row = (DBI_ROW)malloc(sizeof(char *) * num_fields);
      mdb->row_size = num_fields;

      // now reset the row_number now that we have the space allocated
      mdb->row_number = 1;
   }

   // if still within the result set
   if (mdb->row_number <= mdb->num_rows) {
      Dmsg2(500, "my_dbi_fetch_row row number '%d' is acceptable (0..%d)\n", mdb->row_number, mdb->num_rows);
      // get each value from this row
      for (j = 0; j < mdb->num_fields; j++) {
         mdb->row[j] = my_dbi_getvalue(mdb->result, mdb->row_number, j);
         Dmsg2(500, "my_dbi_fetch_row field '%d' has value '%s'\n", j, mdb->row[j]);
      }
      // increment the row number for the next call
      mdb->row_number++;

      row = mdb->row;
   } else {
      Dmsg2(500, "my_dbi_fetch_row row number '%d' is NOT acceptable (0..%d)\n", mdb->row_number, mdb->num_rows);
   }

   Dmsg1(500, "my_dbi_fetch_row finishes returning %p\n", row);

   return row;
}

int my_dbi_max_length(B_DB *mdb, int field_num) {
   //
   // for a given column, find the max length
   //
   int max_length;
   int i;
   int this_length;

   max_length = 0;
   for (i = 0; i < mdb->num_rows; i++) {
      if (my_dbi_getisnull(mdb->result, i, field_num)) {
          this_length = 4;        // "NULL"
      } else {
          // TODO: error
          this_length = cstrlen(my_dbi_getvalue(mdb->result, i, field_num));
      }

      if (max_length < this_length) {
          max_length = this_length;
      }
   }

   return max_length;
}

DBI_FIELD * my_dbi_fetch_field(B_DB *mdb)
{
   int     i;
   int     dbi_index;

   Dmsg0(500, "my_dbi_fetch_field starts\n");

   if (!mdb->fields || mdb->fields_size < mdb->num_fields) {
      if (mdb->fields) {
         free(mdb->fields);
      }
      Dmsg1(500, "allocating space for %d fields\n", mdb->num_fields);
      mdb->fields = (DBI_FIELD *)malloc(sizeof(DBI_FIELD) * mdb->num_fields);
      mdb->fields_size = mdb->num_fields;

      for (i = 0; i < mdb->num_fields; i++) {
         dbi_index = i + 1;
         Dmsg1(500, "filling field %d\n", i);
         mdb->fields[i].name       = (char *)dbi_result_get_field_name(mdb->result, dbi_index);
         mdb->fields[i].max_length = my_dbi_max_length(mdb, i);
         mdb->fields[i].type       = dbi_result_get_field_type_idx(mdb->result, dbi_index);
         mdb->fields[i].flags      = dbi_result_get_field_attribs_idx(mdb->result, dbi_index);

         Dmsg4(500, "my_dbi_fetch_field finds field '%s' has length='%d' type='%d' and IsNull=%d\n",
            mdb->fields[i].name, mdb->fields[i].max_length, mdb->fields[i].type,
            mdb->fields[i].flags);
      } // end for
   } // end if

   // increment field number for the next time around

   Dmsg0(500, "my_dbi_fetch_field finishes\n");
   return &mdb->fields[mdb->field_number++];
}

void my_dbi_data_seek(B_DB *mdb, int row)
{
   // set the row number to be returned on the next call
   // to my_dbi_fetch_row
   mdb->row_number = row;
}

void my_dbi_field_seek(B_DB *mdb, int field)
{
   mdb->field_number = field;
}

/*
 * Note, if this routine returns 1 (failure), Bacula expects
 *  that no result has been stored.
 *
 *  Returns:  0  on success
 *            1  on failure
 *
 */
int my_dbi_query(B_DB *mdb, const char *query)
{
   const char *errmsg;
   Dmsg1(500, "my_dbi_query started %s\n", query);
   // We are starting a new query.  reset everything.
   mdb->num_rows     = -1;
   mdb->row_number   = -1;
   mdb->field_number = -1;

   if (mdb->result) {
      dbi_result_free(mdb->result);  /* hmm, someone forgot to free?? */
      mdb->result = NULL;
   }

   //for (int i=0; i < 10; i++) {
          
      mdb->result = (void **)dbi_conn_query(mdb->db, query);
      
   //   if (mdb->result) {
   //      break;
   //   }
   //   bmicrosleep(5, 0);
   //}
   if (mdb->result == NULL) {
      Dmsg2(50, "Query failed: %s %p\n", query, mdb->result);      
      goto bail_out;
   }

   //mdb->status = (dbi_error_flag)dbi_conn_error_flag(mdb->db);
   mdb->status = DBI_ERROR_NONE;
   
   if (mdb->status == DBI_ERROR_NONE) {
      Dmsg1(500, "we have a result\n", query);

      // how many fields in the set?
      mdb->num_fields = dbi_result_get_numfields(mdb->result);
      Dmsg1(500, "we have %d fields\n", mdb->num_fields);

      mdb->num_rows = dbi_result_get_numrows(mdb->result);
      Dmsg1(500, "we have %d rows\n", mdb->num_rows);

      mdb->status = (dbi_error_flag) 0;                  /* succeed */
   } else {
      Dmsg1(50, "Result status failed: %s\n", query);
      goto bail_out;
   }

   Dmsg0(500, "my_dbi_query finishing\n");
   return mdb->status;

bail_out:
   mdb->status = dbi_conn_error_flag(mdb->db);
   dbi_conn_error(mdb->db, &errmsg);
   Dmsg4(500, "my_dbi_query we failed dbi error "
                   "'%s' '%p' '%d' flag '%d''\n", errmsg, mdb->result, mdb->result, mdb->status);
   dbi_result_free(mdb->result);
   mdb->result = NULL;
   mdb->status = (dbi_error_flag) 1;                   /* failed */
   return mdb->status;
}

void my_dbi_free_result(B_DB *mdb)
{
   int i;
   
   db_lock(mdb);
   //Dmsg2(500, "my_dbi_free_result started result '%p' '%p'\n", mdb->result, mdb->result);
   if (mdb->result != NULL) {
          i = dbi_result_free(mdb->result);
      if(i == 0) {
         mdb->result = NULL;
         //Dmsg2(500, "my_dbi_free_result result '%p' '%d'\n", mdb->result, mdb->result);
      }
      
   }

   if (mdb->row) {
      free(mdb->row);
      mdb->row = NULL;
   }

   if (mdb->fields) {
      free(mdb->fields);
      mdb->fields = NULL;
   }
   db_unlock(mdb);
   //Dmsg0(500, "my_dbi_free_result finish\n");
   
}

const char *my_dbi_strerror(B_DB *mdb) 
{
   const char *errmsg;
   
   dbi_conn_error(mdb->db, &errmsg);
        
   return errmsg;
}

// TODO: make batch insert work with libdbi
#ifdef HAVE_BATCH_FILE_INSERT

int my_dbi_batch_start(JCR *jcr, B_DB *mdb)
{
   char *query = "COPY batch FROM STDIN";

   Dmsg0(500, "my_postgresql_batch_start started\n");

   if (my_postgresql_query(mdb,
                           "CREATE TEMPORARY TABLE batch ("
                               "fileindex int,"
                               "jobid int,"
                               "path varchar,"
                               "name varchar,"
                               "lstat varchar,"
                               "md5 varchar)") == 1)
   {
      Dmsg0(500, "my_postgresql_batch_start failed\n");
      return 1;
   }
   
   // We are starting a new query.  reset everything.
   mdb->num_rows     = -1;
   mdb->row_number   = -1;
   mdb->field_number = -1;

   my_postgresql_free_result(mdb);

   for (int i=0; i < 10; i++) {
      mdb->result = PQexec(mdb->db, query);
      if (mdb->result) {
         break;
      }
      bmicrosleep(5, 0);
   }
   if (!mdb->result) {
      Dmsg1(50, "Query failed: %s\n", query);
      goto bail_out;
   }

   mdb->status = PQresultStatus(mdb->result);
   if (mdb->status == PGRES_COPY_IN) {
      // how many fields in the set?
      mdb->num_fields = (int) PQnfields(mdb->result);
      mdb->num_rows   = 0;
      mdb->status = 1;
   } else {
      Dmsg1(50, "Result status failed: %s\n", query);
      goto bail_out;
   }

   Dmsg0(500, "my_postgresql_batch_start finishing\n");

   return mdb->status;

bail_out:
   Mmsg1(&mdb->errmsg, _("error starting batch mode: %s"), PQerrorMessage(mdb->db));
   mdb->status = 0;
   PQclear(mdb->result);
   mdb->result = NULL;
   return mdb->status;
}

/* set error to something to abort operation */
int my_dbi_batch_end(JCR *jcr, B_DB *mdb, const char *error)
{
   int res;
   int count=30;
   Dmsg0(500, "my_postgresql_batch_end started\n");

   if (!mdb) {                  /* no files ? */
      return 0;
   }

   do { 
      res = PQputCopyEnd(mdb->db, error);
   } while (res == 0 && --count > 0);

   if (res == 1) {
      Dmsg0(500, "ok\n");
      mdb->status = 1;
   }
   
   if (res <= 0) {
      Dmsg0(500, "we failed\n");
      mdb->status = 0;
      Mmsg1(&mdb->errmsg, _("error ending batch mode: %s"), PQerrorMessage(mdb->db));
   }
   
   Dmsg0(500, "my_postgresql_batch_end finishing\n");

   return mdb->status;
}

int my_dbi_batch_insert(JCR *jcr, B_DB *mdb, ATTR_DBR *ar)
{
   int res;
   int count=30;
   size_t len;
   char *digest;
   char ed1[50];

   mdb->esc_name = check_pool_memory_size(mdb->esc_name, mdb->fnl*2+1);
   my_postgresql_copy_escape(mdb->esc_name, mdb->fname, mdb->fnl);

   mdb->esc_path = check_pool_memory_size(mdb->esc_path, mdb->pnl*2+1);
   my_postgresql_copy_escape(mdb->esc_path, mdb->path, mdb->pnl);

   if (ar->Digest == NULL || ar->Digest[0] == 0) {
      digest = "0";
   } else {
      digest = ar->Digest;
   }

   len = Mmsg(mdb->cmd, "%u\t%s\t%s\t%s\t%s\t%s\n", 
              ar->FileIndex, edit_int64(ar->JobId, ed1), mdb->esc_path, 
              mdb->esc_name, ar->attr, digest);

   do { 
      res = PQputCopyData(mdb->db,
                          mdb->cmd,
                          len);
   } while (res == 0 && --count > 0);

   if (res == 1) {
      Dmsg0(500, "ok\n");
      mdb->changes++;
      mdb->status = 1;
   }

   if (res <= 0) {
      Dmsg0(500, "we failed\n");
      mdb->status = 0;
      Mmsg1(&mdb->errmsg, _("error ending batch mode: %s"), PQerrorMessage(mdb->db));
   }

   Dmsg0(500, "my_postgresql_batch_insert finishing\n");

   return mdb->status;
}

#endif /* HAVE_BATCH_FILE_INSERT */

/* my_dbi_getisnull
 * like PQgetisnull
 * int PQgetisnull(const PGresult *res,
 *              int row_number,
 *               int column_number);
 * 
 *  use dbi_result_seek_row to search in result set
 */
int my_dbi_getisnull(dbi_result *result, int row_number, int column_number) {
   int i;

   if(row_number == 0) {
      row_number++;
   }
   
   column_number++;
   
   if(dbi_result_seek_row(result, row_number)) {

      i = dbi_result_field_is_null_idx(result,column_number);

      return i;
   } else {
           
      return 0;
   }
                
}
/* my_dbi_getvalue 
 * like PQgetvalue;
 * char *PQgetvalue(const PGresult *res,
 *                int row_number,
 *                int column_number);
 *
 * use dbi_result_seek_row to search in result set
 * use example to return only strings
 */
char *my_dbi_getvalue(dbi_result *result, int row_number, unsigned int column_number) {

   /* TODO: This is very bad, need refactoring */
   POOLMEM *buf = get_pool_memory(PM_FNAME);
   //const unsigned char *bufb = (unsigned char *)malloc(sizeof(unsigned char) * 300);
   //const unsigned char *bufb;
   const char *errmsg;
   const char *field_name;     
   unsigned short dbitype;
   int32_t field_length = 0;
   int64_t num;
        
   /* correct the index for dbi interface
    * dbi index begins 1
    * I prefer do not change others functions
    */
   Dmsg3(600, "my_dbi_getvalue pre-starting result '%p' row number '%d' column number '%d'\n", 
                                result, row_number, column_number);
        
   column_number++;

   if(row_number == 0) {
     row_number++;
   }
      
   Dmsg3(600, "my_dbi_getvalue starting result '%p' row number '%d' column number '%d'\n", 
                        result, row_number, column_number);
   
   if(dbi_result_seek_row(result, row_number)) {

      field_name = dbi_result_get_field_name(result, column_number);
      field_length = dbi_result_get_field_length(result, field_name);
      dbitype = dbi_result_get_field_type_idx(result,column_number);
                
      if(field_length) {
          buf = check_pool_memory_size(buf, field_length + 1);
      } else {
          buf = check_pool_memory_size(buf, 50);
      }
      
      Dmsg5(500, "my_dbi_getvalue result '%p' type '%d' \n field name '%s' "
            "field_length '%d' field_length size '%d'\n", 
            result, dbitype, field_name, field_length, sizeof_pool_memory(buf));
      
      switch (dbitype) {
      case DBI_TYPE_INTEGER:
         num = dbi_result_get_longlong(result, field_name);         
         edit_int64(num, buf);
         field_length = strlen(buf);
         break;
      case DBI_TYPE_STRING:
         if(field_length) {
            field_length = bsnprintf(buf, field_length + 1, "%s", 
                  dbi_result_get_string(result, field_name));
         } else {
            buf[0] = 0;
         }
         break;
      case DBI_TYPE_BINARY:
         /* dbi_result_get_binary return a NULL pointer if value is empty
         * following, change this to what Bacula espected
         */
         if(field_length) {
            field_length = bsnprintf(buf, field_length + 1, "%s", 
                  dbi_result_get_binary(result, field_name));
         } else {
            buf[0] = 0;
         }
         break;
      case DBI_TYPE_DATETIME:
         time_t last;
         struct tm tm;
         
         last = dbi_result_get_datetime(result, field_name);
         
         if(last == -1) {
                field_length = bsnprintf(buf, 20, "0000-00-00 00:00:00"); 
         } else {
            (void)localtime_r(&last, &tm);
            field_length = bsnprintf(buf, 20, "%04d-%02d-%02d %02d:%02d:%02d",
                  (tm.tm_year + 1900), (tm.tm_mon + 1), tm.tm_mday,
                  tm.tm_hour, tm.tm_min, tm.tm_sec);
         }
         break;
      }

   } else {
      dbi_conn_error(dbi_result_get_conn(result), &errmsg);
      Dmsg1(500, "my_dbi_getvalue error: %s\n", errmsg);
   }
                
   Dmsg3(500, "my_dbi_getvalue finish result '%p' num bytes '%d' data '%s'\n", 
      result, field_length, buf);
   return buf;
}

int my_dbi_sql_insert_id(B_DB *mdb, char *table_name)
{
   /*
    Obtain the current value of the sequence that
    provides the serial value for primary key of the table.

    currval is local to our session.  It is not affected by
    other transactions.

    Determine the name of the sequence.
    PostgreSQL automatically creates a sequence using
    <table>_<column>_seq.
    At the time of writing, all tables used this format for
    for their primary key: <table>id
    Except for basefiles which has a primary key on baseid.
    Therefore, we need to special case that one table.

    everything else can use the PostgreSQL formula.
   */
   
   char      sequence[30];  
   uint64_t    id = 0;

   if (mdb->db_type == SQL_TYPE_POSTGRESQL) {
           
      if (strcasecmp(table_name, "basefiles") == 0) {
         bstrncpy(sequence, "basefiles_baseid", sizeof(sequence));
      } else {
         bstrncpy(sequence, table_name, sizeof(sequence));
         bstrncat(sequence, "_",        sizeof(sequence));
         bstrncat(sequence, table_name, sizeof(sequence));
         bstrncat(sequence, "id",       sizeof(sequence));
      }

      bstrncat(sequence, "_seq", sizeof(sequence));
      id = dbi_conn_sequence_last(mdb->db, NT_(sequence));
   } else {
      id = dbi_conn_sequence_last(mdb->db, NT_(table_name));
   }
   
   return id;
}

#ifdef HAVE_BATCH_FILE_INSERT
const char *my_dbi_batch_lock_path_query = 
   "BEGIN; LOCK TABLE Path IN SHARE ROW EXCLUSIVE MODE";


const char *my_dbi_batch_lock_filename_query = 
   "BEGIN; LOCK TABLE Filename IN SHARE ROW EXCLUSIVE MODE";

const char *my_dbi_batch_unlock_tables_query = "COMMIT";

const char *my_dbi_batch_fill_path_query = 
   "INSERT INTO Path (Path) "
    "SELECT a.Path FROM "
     "(SELECT DISTINCT Path FROM batch) AS a "
      "WHERE NOT EXISTS (SELECT Path FROM Path WHERE Path = a.Path) ";


const char *my_dbi_batch_fill_filename_query = 
   "INSERT INTO Filename (Name) "
    "SELECT a.Name FROM "
     "(SELECT DISTINCT Name FROM batch) as a "
      "WHERE NOT EXISTS "
       "(SELECT Name FROM Filename WHERE Name = a.Name)";
#endif /* HAVE_BATCH_FILE_INSERT */

#endif /* HAVE_DBI */
