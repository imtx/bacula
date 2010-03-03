#include "bacula.h"

#ifdef HAVE_INGRES
EXEC SQL INCLUDE SQLCA;
EXEC SQL INCLUDE SQLDA;

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "myingres.h"

/*
 * ---Implementations---
 */
int INGcheck()
{
   return (sqlca.sqlcode < 0) ? sqlca.sqlcode : 0;
}

short INGgetCols(const char *stmt)
{
   EXEC SQL BEGIN DECLARE SECTION;
   char *stmtd;
   EXEC SQL END DECLARE SECTION;
   
   short number = 1;
   IISQLDA *sqlda;

   sqlda = (IISQLDA *)malloc(IISQDA_HEAD_SIZE + (number * IISQDA_VAR_SIZE));
   memset(sqlda, 0, (IISQDA_HEAD_SIZE + (number * IISQDA_VAR_SIZE)));
   
   sqlda->sqln = number;
   
   stmtd = (char*)malloc(strlen(stmt)+1);
   bstrncpy(stmtd,stmt,strlen(stmt)+1);
     
   EXEC SQL PREPARE s1 from :stmtd;
   if (INGcheck() < 0) {
      free(stmtd);
      free(sqlda);
      return -1;
   }
   EXEC SQL DESCRIBE s1 into :sqlda;
   if (INGcheck() < 0) {
      free(stmtd);
      free(sqlda);
      return -1;
   }
     
   number = sqlda->sqld;
   free(stmtd);
   free(sqlda);
   return number;
}

IISQLDA *INGgetDescriptor(short numCols, const char *stmt)
{
   EXEC SQL BEGIN DECLARE SECTION;
   char *stmtd;
   EXEC SQL END DECLARE SECTION;

   int i;
   IISQLDA *sqlda;

   sqlda = (IISQLDA *)malloc(IISQDA_HEAD_SIZE + (numCols * IISQDA_VAR_SIZE));
   memset(sqlda, 0, (IISQDA_HEAD_SIZE + (numCols * IISQDA_VAR_SIZE)));
   
   sqlda->sqln = numCols;
   
   stmtd = (char *)malloc(strlen(stmt)+1);
   bstrncpy(stmtd,stmt,strlen(stmt)+1);
  
   EXEC SQL PREPARE s2 INTO :sqlda FROM :stmtd;
  
   free(stmtd);

   for (i = 0; i < sqlda->sqld; ++i) {
      /*
       * Alloc space for variable like indicated in sqllen
       * for date types sqllen is always 0 -> allocate by type
       */
      switch (abs(sqlda->sqlvar[i].sqltype)) {
      case IISQ_TSW_TYPE:
         sqlda->sqlvar[i].sqldata = (char *)malloc(IISQ_TSW_LEN);
         break;
      case IISQ_TSWO_TYPE:
         sqlda->sqlvar[i].sqldata = (char *)malloc(IISQ_TSWO_LEN);
         break;
      case IISQ_TSTMP_TYPE:
         sqlda->sqlvar[i].sqldata = (char *)malloc(IISQ_TSTMP_LEN);
         break;
      default:
         sqlda->sqlvar[i].sqldata = (char *)malloc(sqlda->sqlvar[i].sqllen);
         break;
      }
   }
   
   return sqlda;
}

void INGfreeDescriptor(IISQLDA *sqlda)
{
   int i;

   for (i = 0; i < sqlda->sqld; ++i) {
      if (sqlda->sqlvar[i].sqldata) {
         free(sqlda->sqlvar[i].sqldata);
      }
      if (sqlda->sqlvar[i].sqlind) {
         free(sqlda->sqlvar[i].sqlind);
      }
   }
   free(sqlda);
   sqlda = NULL;
}

int INGgetTypeSize(IISQLVAR *ingvar)
{
   int inglength = 0;
   
   /*
    * TODO: add date types (at least TSTMP,TSW TSWO)
    */
   switch (ingvar->sqltype) {
   case IISQ_DTE_TYPE:
      inglength = 25;
      break;
   case IISQ_MNY_TYPE:
      inglength = 8;
      break;
   default:
      inglength = ingvar->sqllen;
      break;
   }
   
   return inglength;
}

INGresult *INGgetINGresult(IISQLDA *sqlda)
{
   int i;
   INGresult *result = NULL;
   
   result = (INGresult *)malloc(sizeof(INGresult));
   memset(result, 0, sizeof(INGresult));
   
   result->sqlda = sqlda;
   result->num_fields = sqlda->sqld;
   result->num_rows = 0;
   result->first_row = NULL;
   result->status = ING_EMPTY_RESULT;
   result->act_row = NULL;
   memset(result->numrowstring, 0, sizeof(result->numrowstring));
   
   if (result->num_fields) {
      result->fields = (INGRES_FIELD *)malloc(sizeof(INGRES_FIELD) * result->num_fields);
      memset(result->fields, 0, sizeof(INGRES_FIELD) * result->num_fields);

      for (i = 0; i < result->num_fields; ++i) {
         memset(result->fields[i].name, 0, 34);
         bstrncpy(result->fields[i].name, sqlda->sqlvar[i].sqlname.sqlnamec, sqlda->sqlvar[i].sqlname.sqlnamel);
         result->fields[i].max_length = INGgetTypeSize(&sqlda->sqlvar[i]);
         result->fields[i].type = abs(sqlda->sqlvar[i].sqltype);
         result->fields[i].flags = (abs(sqlda->sqlvar[i].sqltype)<0) ? 1 : 0;
      }
   }

   return result;
}

void INGfreeINGresult(INGresult *ing_res)
{
   int rows;
   ING_ROW *rowtemp;

   /*
    * Free all rows and fields, then res, not descriptor!
    */
   if (ing_res != NULL) {
      /*
       * Use of rows is a nasty workaround til I find the reason,
       * why aggregates like max() don't work
       */
      rows = ing_res->num_rows;
      ing_res->act_row = ing_res->first_row;
      while (ing_res->act_row != NULL && rows > 0) {
         rowtemp = ing_res->act_row->next;
         INGfreeRowSpace(ing_res->act_row, ing_res->sqlda);
         ing_res->act_row = rowtemp;
         --rows;
      }
      if (ing_res->fields) {
         free(ing_res->fields);
      }
   }
   free(ing_res);
   ing_res = NULL;
}

ING_ROW *INGgetRowSpace(INGresult *ing_res)
{
   int i;
   unsigned short len; /* used for VARCHAR type length */
   IISQLDA *sqlda = ing_res->sqlda;
   ING_ROW *row = NULL;
   IISQLVAR *vars = NULL;

   row = (ING_ROW *)malloc(sizeof(ING_ROW));
   memset(row, 0, sizeof(ING_ROW));

   vars = (IISQLVAR *)malloc(sizeof(IISQLVAR) * sqlda->sqld);
   memset(vars, 0, sizeof(IISQLVAR) * sqlda->sqld);

   row->sqlvar = vars;
   row->next = NULL;

   for (i = 0; i < sqlda->sqld; ++i) {
      /*
       * Make strings out of the data, then the space and assign 
       * (why string? at least it seems that way, looking into the sources)
       */
      switch (ing_res->fields[i].type) {
      case IISQ_VCH_TYPE:
         len = ((ING_VARCHAR *)sqlda->sqlvar[i].sqldata)->len;
         vars[i].sqldata = (char *)malloc(len+1);
         memcpy(vars[i].sqldata,sqlda->sqlvar[i].sqldata+2,len);
         vars[i].sqldata[len] = '\0';
         break;
      case IISQ_CHA_TYPE:
         vars[i].sqldata = (char *)malloc(ing_res->fields[i].max_length+1);
         memcpy(vars[i].sqldata,sqlda->sqlvar[i].sqldata,sqlda->sqlvar[i].sqllen);
         vars[i].sqldata[ing_res->fields[i].max_length] = '\0';
         break;
      case IISQ_INT_TYPE:
         vars[i].sqldata = (char *)malloc(20);
         memset(vars[i].sqldata, 0, 20);
         switch (sqlda->sqlvar[i].sqllen) {
         case 2:
            bsnprintf(vars[i].sqldata, 20, "%d",*(short*)sqlda->sqlvar[i].sqldata);
            break;
         case 4:
            bsnprintf(vars[i].sqldata, 20, "%ld",*(int*)sqlda->sqlvar[i].sqldata);
            break;
         case 8:
            bsnprintf(vars[i].sqldata, 20, "%lld",*(long*)sqlda->sqlvar[i].sqldata);
            break;
         }
         break;
      case IISQ_TSTMP_TYPE:
         vars[i].sqldata = (char *)malloc(IISQ_TSTMP_LEN+1);
         vars[i].sqldata[IISQ_TSTMP_LEN] = '\0';
         break;
      case IISQ_TSWO_TYPE:
         vars[i].sqldata = (char *)malloc(IISQ_TSWO_LEN+1);
         vars[i].sqldata[IISQ_TSWO_LEN] = '\0';
         break;
      case IISQ_TSW_TYPE:
         vars[i].sqldata = (char *)malloc(IISQ_TSW_LEN+1);
         vars[i].sqldata[IISQ_TSW_LEN] = '\0';
         break;
      }
      vars[i].sqlind = (short *)malloc(sizeof(short));
      if (sqlda->sqlvar[i].sqlind) {
         memcpy(vars[i].sqlind,sqlda->sqlvar[i].sqlind,sizeof(short));
      } else {
         *vars[i].sqlind = 0;
      }
   }
   
   return row;
}


void INGfreeRowSpace(ING_ROW *row, IISQLDA *sqlda)
{
   int i;

   if (row == NULL || sqlda == NULL) {
      return;
   }

   for (i = 0; i < sqlda->sqld; ++i) {
      if (row->sqlvar[i].sqldata) {
         free(row->sqlvar[i].sqldata);
      }
      if (row->sqlvar[i].sqlind) {
         free(row->sqlvar[i].sqlind);
      }
   }
   free(row->sqlvar);
   free(row);
}

int INGfetchAll(const char *stmt, INGresult *ing_res)
{
   int linecount = 0;
   ING_ROW *row;
   IISQLDA *desc;
   int check = -1;
   
   desc = ing_res->sqlda;
   
   EXEC SQL DECLARE c2 CURSOR FOR s2;
   if ((check = INGcheck()) < 0) {
      return check;
   }
   
   EXEC SQL OPEN c2;
   if ((check = INGcheck()) < 0) {
      return check;
   }
      
   /* for (linecount = 0; sqlca.sqlcode == 0; ++linecount) */
   while(sqlca.sqlcode == 0) {
      EXEC SQL FETCH c2 USING DESCRIPTOR :desc;
      if ((check = INGcheck()) < 0) {
         EXEC SQL CLOSE c2;
         return check;
      }

      if (sqlca.sqlcode == 0) {
         row = INGgetRowSpace(ing_res); /* alloc space for fetched row */
            
         /*
          * Initialize list when encountered first time
          */
         if (ing_res->first_row == 0) {
            ing_res->first_row = row; /* head of the list */
            ing_res->first_row->next = NULL;
            ing_res->act_row = ing_res->first_row;
         }      
         ing_res->act_row->next = row; /* append row to old act_row */
         ing_res->act_row = row; /* set row as act_row */
         row->row_number = linecount;
         ++linecount;
      }
   }
   
   EXEC SQL CLOSE c2;
   
   ing_res->status = ING_COMMAND_OK;
   ing_res->num_rows = linecount;
   return linecount;
}

ING_STATUS INGresultStatus(INGresult *res)
{
   if (res == NULL) {return ING_NO_RESULT;}
   return res->status;
}

void INGrowSeek(INGresult *res, int row_number)
{
   ING_ROW *trow;
   if (res->act_row->row_number == row_number) {
      return;
   }
   
   /*
    * TODO: real error handling
    */
   if (row_number<0 || row_number>res->num_rows) {
      return;
   }

   for (trow = res->first_row; trow->row_number != row_number; trow = trow->next) ;
   res->act_row = trow;
   /*
    * Note - can be null - if row_number not found, right?
    */
}

char *INGgetvalue(INGresult *res, int row_number, int column_number)
{
   if (row_number != res->act_row->row_number) {
      INGrowSeek(res, row_number);
   }
   return res->act_row->sqlvar[column_number].sqldata;
}

int INGgetisnull(INGresult *res, int row_number, int column_number)
{
   if (row_number != res->act_row->row_number) {
      INGrowSeek(res, row_number);
   }
   return (short)*res->act_row->sqlvar[column_number].sqlind;
}

int INGntuples(const INGresult *res)
{
   return res->num_rows;
}

int INGnfields(const INGresult *res)
{
   return res->num_fields;
}

char *INGfname(const INGresult *res, int column_number)
{
   if ((column_number > res->num_fields) || (column_number < 0)) {
      return NULL;
   } else {
      return res->fields[column_number].name;
   }
}

short INGftype(const INGresult *res, int column_number)
{
   return res->fields[column_number].type;
}

int INGexec(INGconn *conn, const char *query)
{
   int check;
   EXEC SQL BEGIN DECLARE SECTION;
   int rowcount;
   char *stmt;
   EXEC SQL END DECLARE SECTION;
   
   stmt = (char *)malloc(strlen(query)+1);
   bstrncpy(stmt,query,strlen(query)+1);
   rowcount = -1;

   EXEC SQL EXECUTE IMMEDIATE :stmt;
   free(stmt);
   if ((check = INGcheck()) < 0) {
      return check;
   }

   EXEC SQL INQUIRE_INGRES(:rowcount = ROWCOUNT);
   if ((check = INGcheck()) < 0) {
      return check;
   }
   
   return rowcount;
}

INGresult *INGquery(INGconn *conn, const char *query)
{
   /*
    * TODO: error handling
    */
   IISQLDA *desc = NULL;
   INGresult *res = NULL;
   int rows = -1;
   int cols = INGgetCols(query);

   desc = INGgetDescriptor(cols, query);
   res = INGgetINGresult(desc);
   rows = INGfetchAll(query, res);

   if (rows < 0) {
     INGfreeINGresult(res);
     return NULL;
   }
   return res;
}

void INGclear(INGresult *res)
{
   if (res == NULL) {
      return;
   }
   IISQLDA *desc = res->sqlda;
   INGfreeINGresult(res);
   INGfreeDescriptor(desc);
}

INGconn *INGconnectDB(char *dbname, char *user, char *passwd)
{
   if (dbname == NULL || strlen(dbname) == 0) {
      return NULL;
   }

   INGconn *dbconn = (INGconn *)malloc(sizeof(INGconn));
   memset(dbconn, 0, sizeof(INGconn));

   EXEC SQL BEGIN DECLARE SECTION;
   char ingdbname[24];
   char ingdbuser[32];
   char ingdbpasw[32];
   char conn_name[32];
   int sess_id;
   EXEC SQL END DECLARE SECTION;

   bstrncpy(ingdbname, dbname, sizeof(ingdbname));
   
   if (user != NULL) {
      bstrncpy(ingdbuser, user, sizeof(ingdbuser));
      if (passwd != NULL) {
         bstrncpy(ingdbpasw, passwd, sizeof(ingdbpasw));
      } else {
         memset(ingdbpasw, 0, sizeof(ingdbpasw));
      }
      EXEC SQL CONNECT
         :ingdbname
         identified by :ingdbuser
         dbms_password = :ingdbpasw;
   } else {
      EXEC SQL CONNECT :ingdbname;
   }   
   
   EXEC SQL INQUIRE_SQL(:conn_name = connection_name);
   EXEC SQL INQUIRE_SQL(:sess_id = session);
   
   bstrncpy(dbconn->dbname, ingdbname, sizeof(dbconn->dbname));
   bstrncpy(dbconn->user, ingdbuser, sizeof(dbconn->user));
   bstrncpy(dbconn->password, ingdbpasw, sizeof(dbconn->password));
   bstrncpy(dbconn->connection_name, conn_name, sizeof(dbconn->connection_name));
   dbconn->session_id = sess_id;
   dbconn->msg = (char*)malloc(257);
   memset(dbconn->msg, 0, 257);

   return dbconn;
}

void INGdisconnectDB(INGconn *dbconn)
{
   /*
    * TODO: check for any real use of dbconn: maybe whenn multithreaded?
    */
   EXEC SQL DISCONNECT;
   if (dbconn != NULL) {
      free(dbconn->msg);
      free(dbconn);
   }
}

char *INGerrorMessage(const INGconn *conn)
{
   EXEC SQL BEGIN DECLARE SECTION;
   char errbuf[256];
   EXEC SQL END DECLARE SECTION;

   EXEC SQL INQUIRE_INGRES(:errbuf = ERRORTEXT);
   memcpy(conn->msg,&errbuf,256);
   return conn->msg;
}

char *INGcmdTuples(INGresult *res)
{
   return res->numrowstring;
}

/* TODO?
int INGputCopyEnd(INGconn *conn, const char *errormsg);
int INGputCopyData(INGconn *conn, const char *buffer, int nbytes);
*/

#endif
