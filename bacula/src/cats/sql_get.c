/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2014 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from many
   others, a complete list can be found in the file AUTHORS.

   You may use this file and others of this release according to the
   license defined in the LICENSE file, which includes the Affero General
   Public License, v3.0 ("AGPLv3") and some additional permissions and
   terms pursuant to its AGPLv3 Section 7.

   Bacula® is a registered trademark of Kern Sibbald.
*/
/**
 * Bacula Catalog Database Get record interface routines
 *  Note, these routines generally get a record by id or
 *        by name.  If more logic is involved, the routine
 *        should be in find.c
 *
 *    Written by Kern Sibbald, March 2000
 *
 */

#include "bacula.h"

#if HAVE_SQLITE3 || HAVE_MYSQL || HAVE_POSTGRESQL

#include "cats.h"
#include "bdb_priv.h"
#include "sql_glue.h"

/* -----------------------------------------------------------------------
 *
 *   Generic Routines (or almost generic)
 *
 * -----------------------------------------------------------------------
 */

/* Forward referenced functions */
static int db_get_file_record(JCR *jcr, B_DB *mdb, JOB_DBR *jr, FILE_DBR *fdbr);
static int db_get_filename_record(JCR *jcr, B_DB *mdb);


/**
 * Given a full filename (with path), look up the File record
 * (with attributes) in the database.
 *
 *  Returns: 0 on failure
 *           1 on success with the File record in FILE_DBR
 */
int db_get_file_attributes_record(JCR *jcr, B_DB *mdb, char *fname, JOB_DBR *jr, FILE_DBR *fdbr)
{
   int stat;
   Dmsg1(100, "db_get_file_att_record fname=%s \n", fname);

   db_lock(mdb);
   split_path_and_file(jcr, mdb, fname);

   fdbr->FilenameId = db_get_filename_record(jcr, mdb);

   fdbr->PathId = db_get_path_record(jcr, mdb);

   stat = db_get_file_record(jcr, mdb, jr, fdbr);

   db_unlock(mdb);

   return stat;
}


/**
 * Get a File record
 * Returns: 0 on failure
 *          1 on success
 *
 *  DO NOT use Jmsg in this routine.
 *
 *  Note in this routine, we do not use Jmsg because it may be
 *    called to get attributes of a non-existent file, which is
 *    "normal" if a new file is found during Verify.
 *
 *  The following is a bit of a kludge: because we always backup a
 *    directory entry, we can end up with two copies of the directory
 *    in the backup. One is when we encounter the directory and find
 *    we cannot recurse into it, and the other is when we find an
 *    explicit mention of the directory. This can also happen if the
 *    use includes the directory twice.  In this case, Verify
 *    VolumeToCatalog fails because we have two copies in the catalog,
 *    and only the first one is marked (twice).  So, when calling from Verify,
 *    VolumeToCatalog jr is not NULL and we know jr->FileIndex is the fileindex
 *    of the version of the directory/file we actually want and do
 *    a more explicit SQL search.
 */
static
int db_get_file_record(JCR *jcr, B_DB *mdb, JOB_DBR *jr, FILE_DBR *fdbr)
{
   SQL_ROW row;
   int stat = 0;
   char ed1[50], ed2[50], ed3[50];
   int num_rows;

   if (jcr->getJobLevel() == L_VERIFY_DISK_TO_CATALOG) {
      Mmsg(mdb->cmd,
"SELECT FileId, LStat, MD5 FROM File,Job WHERE "
"File.JobId=Job.JobId AND File.PathId=%s AND "
"File.FilenameId=%s AND Job.Type='B' AND Job.JobStatus IN ('T','W') AND "
"ClientId=%s ORDER BY StartTime DESC LIMIT 1",
      edit_int64(fdbr->PathId, ed1),
      edit_int64(fdbr->FilenameId, ed2),
      edit_int64(jr->ClientId,ed3));
   } else if (jcr->getJobLevel() == L_VERIFY_VOLUME_TO_CATALOG) {
      Mmsg(mdb->cmd,
           "SELECT FileId, LStat, MD5 FROM File WHERE File.JobId=%s AND File.PathId=%s AND "
           "File.FilenameId=%s AND File.FileIndex=%u",
           edit_int64(fdbr->JobId, ed1),
           edit_int64(fdbr->PathId, ed2),
           edit_int64(fdbr->FilenameId,ed3),
           jr->FileIndex);
   } else {
      Mmsg(mdb->cmd,
"SELECT FileId, LStat, MD5 FROM File WHERE File.JobId=%s AND File.PathId=%s AND "
"File.FilenameId=%s",
      edit_int64(fdbr->JobId, ed1),
      edit_int64(fdbr->PathId, ed2),
      edit_int64(fdbr->FilenameId,ed3));
   }
   Dmsg3(450, "Get_file_record JobId=%u FilenameId=%u PathId=%u\n",
      fdbr->JobId, fdbr->FilenameId, fdbr->PathId);

   Dmsg1(100, "Query=%s\n", mdb->cmd);

   if (QUERY_DB(jcr, mdb, mdb->cmd)) {
      num_rows = sql_num_rows(mdb);
      Dmsg1(050, "get_file_record num_rows=%d\n", num_rows);
      if (num_rows >= 1) {
         if ((row = sql_fetch_row(mdb)) == NULL) {
            Mmsg1(mdb->errmsg, _("Error fetching row: %s\n"), sql_strerror(mdb));
         } else {
            fdbr->FileId = (FileId_t)str_to_int64(row[0]);
            bstrncpy(fdbr->LStat, row[1], sizeof(fdbr->LStat));
            bstrncpy(fdbr->Digest, row[2], sizeof(fdbr->Digest));
            stat = 1;
            if (num_rows > 1) {
               Mmsg3(mdb->errmsg, _("get_file_record want 1 got rows=%d PathId=%s FilenameId=%s\n"),
                  num_rows,
                  edit_int64(fdbr->PathId, ed1),
                  edit_int64(fdbr->FilenameId, ed2));
               Dmsg1(000, "=== Problem!  %s", mdb->errmsg);
            }
         }
      } else {
         Mmsg2(mdb->errmsg, _("File record for PathId=%s FilenameId=%s not found.\n"),
            edit_int64(fdbr->PathId, ed1),
            edit_int64(fdbr->FilenameId, ed2));
      }
      sql_free_result(mdb);
   } else {
      Mmsg(mdb->errmsg, _("File record not found in Catalog.\n"));
   }
   return stat;

}

/**
 * Get Filename record
 * Returns: 0 on failure
 *          FilenameId on success
 *
 *   DO NOT use Jmsg in this routine (see notes for get_file_record)
 */
static int db_get_filename_record(JCR *jcr, B_DB *mdb)
{
   SQL_ROW row;
   int FilenameId = 0;
   int num_rows;

   mdb->esc_name = check_pool_memory_size(mdb->esc_name, 2*mdb->fnl+2);
   db_escape_string(jcr, mdb, mdb->esc_name, mdb->fname, mdb->fnl);

   Mmsg(mdb->cmd, "SELECT FilenameId FROM Filename WHERE Name='%s'", mdb->esc_name);
   if (QUERY_DB(jcr, mdb, mdb->cmd)) {
      char ed1[30];
      num_rows = sql_num_rows(mdb);
      if (num_rows > 1) {
         Mmsg2(mdb->errmsg, _("More than one Filename!: %s for file: %s\n"),
            edit_uint64(num_rows, ed1), mdb->fname);
         Jmsg(jcr, M_WARNING, 0, "%s", mdb->errmsg);
      }
      if (num_rows >= 1) {
         if ((row = sql_fetch_row(mdb)) == NULL) {
            Mmsg1(mdb->errmsg, _("error fetching row: %s\n"), sql_strerror(mdb));
         } else {
            FilenameId = str_to_int64(row[0]);
            if (FilenameId <= 0) {
               Mmsg2(mdb->errmsg, _("Get DB Filename record %s found bad record: %d\n"),
                  mdb->cmd, FilenameId);
               FilenameId = 0;
            }
         }
      } else {
         Mmsg1(mdb->errmsg, _("Filename record: %s not found.\n"), mdb->fname);
      }
      sql_free_result(mdb);
   } else {
      Mmsg(mdb->errmsg, _("Filename record: %s not found in Catalog.\n"), mdb->fname);
   }
   return FilenameId;
}

/**
 * Get path record
 * Returns: 0 on failure
 *          PathId on success
 *
 *   DO NOT use Jmsg in this routine (see notes for get_file_record)
 */
int db_get_path_record(JCR *jcr, B_DB *mdb)
{
   SQL_ROW row;
   uint32_t PathId = 0;
   int num_rows;

   mdb->esc_name = check_pool_memory_size(mdb->esc_name, 2*mdb->pnl+2);
   db_escape_string(jcr, mdb, mdb->esc_name, mdb->path, mdb->pnl);

   if (mdb->cached_path_id != 0 && mdb->cached_path_len == mdb->pnl &&
       strcmp(mdb->cached_path, mdb->path) == 0) {
      return mdb->cached_path_id;
   }

   Mmsg(mdb->cmd, "SELECT PathId FROM Path WHERE Path='%s'", mdb->esc_name);

   if (QUERY_DB(jcr, mdb, mdb->cmd)) {
      char ed1[30];
      num_rows = sql_num_rows(mdb);
      if (num_rows > 1) {
         Mmsg2(mdb->errmsg, _("More than one Path!: %s for path: %s\n"),
            edit_uint64(num_rows, ed1), mdb->path);
         Jmsg(jcr, M_WARNING, 0, "%s", mdb->errmsg);
      }
      /* Even if there are multiple paths, take the first one */
      if (num_rows >= 1) {
         if ((row = sql_fetch_row(mdb)) == NULL) {
            Mmsg1(mdb->errmsg, _("error fetching row: %s\n"), sql_strerror(mdb));
         } else {
            PathId = str_to_int64(row[0]);
            if (PathId <= 0) {
               Mmsg2(mdb->errmsg, _("Get DB path record %s found bad record: %s\n"),
                  mdb->cmd, edit_int64(PathId, ed1));
               PathId = 0;
            } else {
               /* Cache path */
               if (PathId != mdb->cached_path_id) {
                  mdb->cached_path_id = PathId;
                  mdb->cached_path_len = mdb->pnl;
                  pm_strcpy(mdb->cached_path, mdb->path);
               }
            }
         }
      } else {
         Mmsg1(mdb->errmsg, _("Path record: %s not found.\n"), mdb->path);
      }
      sql_free_result(mdb);
   } else {
      Mmsg(mdb->errmsg, _("Path record: %s not found in Catalog.\n"), mdb->path);
   }
   return PathId;
}


/**
 * Get Job record for given JobId or Job name
 * Returns: false on failure
 *          true  on success
 */
bool db_get_job_record(JCR *jcr, B_DB *mdb, JOB_DBR *jr)
{
   SQL_ROW row;
   char ed1[50];
   char esc[MAX_ESCAPE_NAME_LENGTH];

   db_lock(mdb);
   if (jr->JobId == 0) {
      mdb->db_escape_string(jcr, esc, jr->Job, strlen(jr->Job));
      Mmsg(mdb->cmd, "SELECT VolSessionId,VolSessionTime,"
"PoolId,StartTime,EndTime,JobFiles,JobBytes,JobTDate,Job,JobStatus,"
"Type,Level,ClientId,Name,PriorJobId,RealEndTime,JobId,FileSetId,"
"SchedTime,RealEndTime,ReadBytes,HasBase,PurgedFiles "
"FROM Job WHERE Job='%s'", esc);
    } else {
      Mmsg(mdb->cmd, "SELECT VolSessionId,VolSessionTime,"
"PoolId,StartTime,EndTime,JobFiles,JobBytes,JobTDate,Job,JobStatus,"
"Type,Level,ClientId,Name,PriorJobId,RealEndTime,JobId,FileSetId,"
"SchedTime,RealEndTime,ReadBytes,HasBase,PurgedFiles "
"FROM Job WHERE JobId=%s",
          edit_int64(jr->JobId, ed1));
    }

   if (!QUERY_DB(jcr, mdb, mdb->cmd)) {
      db_unlock(mdb);
      return false;                   /* failed */
   }
   if ((row = sql_fetch_row(mdb)) == NULL) {
      Mmsg1(mdb->errmsg, _("No Job found for JobId %s\n"), edit_int64(jr->JobId, ed1));
      sql_free_result(mdb);
      db_unlock(mdb);
      return false;                   /* failed */
   }

   jr->VolSessionId = str_to_uint64(row[0]);
   jr->VolSessionTime = str_to_uint64(row[1]);
   jr->PoolId = str_to_int64(row[2]);
   bstrncpy(jr->cStartTime, row[3]!=NULL?row[3]:"", sizeof(jr->cStartTime));
   bstrncpy(jr->cEndTime, row[4]!=NULL?row[4]:"", sizeof(jr->cEndTime));
   jr->JobFiles = str_to_int64(row[5]);
   jr->JobBytes = str_to_int64(row[6]);
   jr->JobTDate = str_to_int64(row[7]);
   bstrncpy(jr->Job, row[8]!=NULL?row[8]:"", sizeof(jr->Job));
   jr->JobStatus = row[9]!=NULL?(int)*row[9]:JS_FatalError;
   jr->JobType = row[10]!=NULL?(int)*row[10]:JT_BACKUP;
   jr->JobLevel = row[11]!=NULL?(int)*row[11]:L_NONE;
   jr->ClientId = str_to_uint64(row[12]!=NULL?row[12]:(char *)"");
   bstrncpy(jr->Name, row[13]!=NULL?row[13]:"", sizeof(jr->Name));
   jr->PriorJobId = str_to_uint64(row[14]!=NULL?row[14]:(char *)"");
   bstrncpy(jr->cRealEndTime, row[15]!=NULL?row[15]:"", sizeof(jr->cRealEndTime));
   if (jr->JobId == 0) {
      jr->JobId = str_to_int64(row[16]);
   }
   jr->FileSetId = str_to_int64(row[17]);
   bstrncpy(jr->cSchedTime, row[3]!=NULL?row[18]:"", sizeof(jr->cSchedTime));
   bstrncpy(jr->cRealEndTime, row[3]!=NULL?row[19]:"", sizeof(jr->cRealEndTime));
   jr->ReadBytes = str_to_int64(row[20]);
   jr->StartTime = str_to_utime(jr->cStartTime);
   jr->SchedTime = str_to_utime(jr->cSchedTime);
   jr->EndTime = str_to_utime(jr->cEndTime);
   jr->RealEndTime = str_to_utime(jr->cRealEndTime);
   jr->HasBase = str_to_int64(row[21]);
   jr->PurgedFiles = str_to_int64(row[22]);
   sql_free_result(mdb);

   db_unlock(mdb);
   return true;
}

/**
 * Find VolumeNames for a given JobId
 *  Returns: 0 on error or no Volumes found
 *           number of volumes on success
 *              Volumes are concatenated in VolumeNames
 *              separated by a vertical bar (|) in the order
 *              that they were written.
 *
 *  Returns: number of volumes on success
 */
int db_get_job_volume_names(JCR *jcr, B_DB *mdb, JobId_t JobId, POOLMEM **VolumeNames)
{
   SQL_ROW row;
   char ed1[50];
   int stat = 0;
   int i;
   int num_rows;

   db_lock(mdb);
   /* Get one entry per VolumeName, but "sort" by VolIndex */
   Mmsg(mdb->cmd,
        "SELECT VolumeName,MAX(VolIndex) FROM JobMedia,Media WHERE "
        "JobMedia.JobId=%s AND JobMedia.MediaId=Media.MediaId "
        "GROUP BY VolumeName "
        "ORDER BY 2 ASC", edit_int64(JobId,ed1));

   Dmsg1(130, "VolNam=%s\n", mdb->cmd);
   *VolumeNames[0] = 0;
   if (QUERY_DB(jcr, mdb, mdb->cmd)) {
      num_rows = sql_num_rows(mdb);
      Dmsg1(130, "Num rows=%d\n", num_rows);
      if (num_rows <= 0) {
         Mmsg1(mdb->errmsg, _("No volumes found for JobId=%d\n"), JobId);
         stat = 0;
      } else {
         stat = num_rows;
         for (i=0; i < stat; i++) {
            if ((row = sql_fetch_row(mdb)) == NULL) {
               Mmsg2(mdb->errmsg, _("Error fetching row %d: ERR=%s\n"), i, sql_strerror(mdb));
               Jmsg(jcr, M_ERROR, 0, "%s", mdb->errmsg);
               stat = 0;
               break;
            } else {
               if (*VolumeNames[0] != 0) {
                  pm_strcat(VolumeNames, "|");
               }
               pm_strcat(VolumeNames, row[0]);
            }
         }
      }
      sql_free_result(mdb);
   } else {
      Mmsg(mdb->errmsg, _("No Volume for JobId %d found in Catalog.\n"), JobId);
   }
   db_unlock(mdb);
   return stat;
}

/**
 * Find Volume parameters for a give JobId
 *  Returns: 0 on error or no Volumes found
 *           number of volumes on success
 *           List of Volumes and start/end file/blocks (malloced structure!)
 *
 *  Returns: number of volumes on success
 */
int db_get_job_volume_parameters(JCR *jcr, B_DB *mdb, JobId_t JobId, VOL_PARAMS **VolParams)
{
   SQL_ROW row;
   char ed1[50];
   int stat = 0;
   int i;
   VOL_PARAMS *Vols = NULL;
   int num_rows;

   db_lock(mdb);
   Mmsg(mdb->cmd,
"SELECT VolumeName,MediaType,FirstIndex,LastIndex,StartFile,"
"JobMedia.EndFile,StartBlock,JobMedia.EndBlock,"
"Slot,StorageId,InChanger"
" FROM JobMedia,Media WHERE JobMedia.JobId=%s"
" AND JobMedia.MediaId=Media.MediaId ORDER BY VolIndex,JobMediaId",
        edit_int64(JobId, ed1));

   Dmsg1(130, "VolNam=%s\n", mdb->cmd);
   if (QUERY_DB(jcr, mdb, mdb->cmd)) {
      num_rows = sql_num_rows(mdb);
      Dmsg1(200, "Num rows=%d\n", num_rows);
      if (num_rows <= 0) {
         Mmsg1(mdb->errmsg, _("No volumes found for JobId=%d\n"), JobId);
         stat = 0;
      } else {
         stat = num_rows;
         DBId_t *SId = NULL;
         if (stat > 0) {
            *VolParams = Vols = (VOL_PARAMS *)malloc(stat * sizeof(VOL_PARAMS));
            SId = (DBId_t *)malloc(stat * sizeof(DBId_t));
         }
         for (i=0; i < stat; i++) {
            if ((row = sql_fetch_row(mdb)) == NULL) {
               Mmsg2(mdb->errmsg, _("Error fetching row %d: ERR=%s\n"), i, sql_strerror(mdb));
               Jmsg(jcr, M_ERROR, 0, "%s", mdb->errmsg);
               stat = 0;
               break;
            } else {
               DBId_t StorageId;
               uint32_t StartBlock, EndBlock, StartFile, EndFile;
               bstrncpy(Vols[i].VolumeName, row[0], MAX_NAME_LENGTH);
               bstrncpy(Vols[i].MediaType, row[1], MAX_NAME_LENGTH);
               Vols[i].FirstIndex = str_to_uint64(row[2]);
               Vols[i].LastIndex = str_to_uint64(row[3]);
               StartFile = str_to_uint64(row[4]);
               EndFile = str_to_uint64(row[5]);
               StartBlock = str_to_uint64(row[6]);
               EndBlock = str_to_uint64(row[7]);
               Vols[i].StartAddr = (((uint64_t)StartFile)<<32) | StartBlock;
               Vols[i].EndAddr =   (((uint64_t)EndFile)<<32) | EndBlock;
               Vols[i].Slot = str_to_uint64(row[8]);
               StorageId = str_to_uint64(row[9]);
               Vols[i].InChanger = str_to_uint64(row[10]);
               Vols[i].Storage[0] = 0;
               SId[i] = StorageId;
            }
         }
         for (i=0; i < stat; i++) {
            if (SId[i] != 0) {
               Mmsg(mdb->cmd, "SELECT Name from Storage WHERE StorageId=%s",
                  edit_int64(SId[i], ed1));
               if (QUERY_DB(jcr, mdb, mdb->cmd)) {
                  if ((row = sql_fetch_row(mdb)) && row[0]) {
                     bstrncpy(Vols[i].Storage, row[0], MAX_NAME_LENGTH);
                  }
               }
            }
         }
         if (SId) {
            free(SId);
         }
      }
      sql_free_result(mdb);
   }
   db_unlock(mdb);
   return stat;
}



/**
 * Get the number of pool records
 *
 * Returns: -1 on failure
 *          number on success
 */
int db_get_num_pool_records(JCR *jcr, B_DB *mdb)
{
   int stat = 0;

   db_lock(mdb);
   Mmsg(mdb->cmd, "SELECT count(*) from Pool");
   stat = get_sql_record_max(jcr, mdb);
   db_unlock(mdb);
   return stat;
}

/**
 * This function returns a list of all the Pool record ids.
 *  The caller must free ids if non-NULL.
 *
 *  Returns 0: on failure
 *          1: on success
 */
int db_get_pool_ids(JCR *jcr, B_DB *mdb, int *num_ids, uint32_t *ids[])
{
   SQL_ROW row;
   int stat = 0;
   int i = 0;
   uint32_t *id;

   db_lock(mdb);
   *ids = NULL;
   Mmsg(mdb->cmd, "SELECT PoolId FROM Pool");
   if (QUERY_DB(jcr, mdb, mdb->cmd)) {
      *num_ids = sql_num_rows(mdb);
      if (*num_ids > 0) {
         id = (uint32_t *)malloc(*num_ids * sizeof(uint32_t));
         while ((row = sql_fetch_row(mdb)) != NULL) {
            id[i++] = str_to_uint64(row[0]);
         }
         *ids = id;
      }
      sql_free_result(mdb);
      stat = 1;
   } else {
      Mmsg(mdb->errmsg, _("Pool id select failed: ERR=%s\n"), sql_strerror(mdb));
      Jmsg(jcr, M_ERROR, 0, "%s", mdb->errmsg);
      stat = 0;
   }
   db_unlock(mdb);
   return stat;
}

/**
 * This function returns a list of all the Client record ids.
 *  The caller must free ids if non-NULL.
 *
 *  Returns 0: on failure
 *          1: on success
 */
int db_get_client_ids(JCR *jcr, B_DB *mdb, int *num_ids, uint32_t *ids[])
{
   SQL_ROW row;
   int stat = 0;
   int i = 0;
   uint32_t *id;

   db_lock(mdb);
   *ids = NULL;
   Mmsg(mdb->cmd, "SELECT ClientId FROM Client ORDER BY Name");
   if (QUERY_DB(jcr, mdb, mdb->cmd)) {
      *num_ids = sql_num_rows(mdb);
      if (*num_ids > 0) {
         id = (uint32_t *)malloc(*num_ids * sizeof(uint32_t));
         while ((row = sql_fetch_row(mdb)) != NULL) {
            id[i++] = str_to_uint64(row[0]);
         }
         *ids = id;
      }
      sql_free_result(mdb);
      stat = 1;
   } else {
      Mmsg(mdb->errmsg, _("Client id select failed: ERR=%s\n"), sql_strerror(mdb));
      Jmsg(jcr, M_ERROR, 0, "%s", mdb->errmsg);
      stat = 0;
   }
   db_unlock(mdb);
   return stat;
}

/**
 * Get Pool Id, Scratch Pool Id, Recycle Pool Id
 * Returns: false on failure
 *          true on success
 */
bool db_get_pool_record(JCR *jcr, B_DB *mdb, POOL_DBR *pdbr)
{
   SQL_ROW row;
   bool ok = false;
   char ed1[50];
   int num_rows;
   char esc[MAX_ESCAPE_NAME_LENGTH];

   db_lock(mdb);
   if (pdbr->PoolId != 0) {               /* find by id */
      Mmsg(mdb->cmd,
"SELECT PoolId,Name,NumVols,MaxVols,UseOnce,UseCatalog,AcceptAnyVolume,"
"AutoPrune,Recycle,VolRetention,VolUseDuration,MaxVolJobs,MaxVolFiles,"
"MaxVolBytes,PoolType,LabelType,LabelFormat,RecyclePoolId,ScratchPoolId,"
"ActionOnPurge FROM Pool WHERE Pool.PoolId=%s",
         edit_int64(pdbr->PoolId, ed1));
   } else {                           /* find by name */
      mdb->db_escape_string(jcr, esc, pdbr->Name, strlen(pdbr->Name));
      Mmsg(mdb->cmd,
"SELECT PoolId,Name,NumVols,MaxVols,UseOnce,UseCatalog,AcceptAnyVolume,"
"AutoPrune,Recycle,VolRetention,VolUseDuration,MaxVolJobs,MaxVolFiles,"
"MaxVolBytes,PoolType,LabelType,LabelFormat,RecyclePoolId,ScratchPoolId,"
"ActionOnPurge FROM Pool WHERE Pool.Name='%s'", esc);
   }
   if (QUERY_DB(jcr, mdb, mdb->cmd)) {
      num_rows = sql_num_rows(mdb);
      if (num_rows > 1) {
         char ed1[30];
         Mmsg1(mdb->errmsg, _("More than one Pool!: %s\n"),
            edit_uint64(num_rows, ed1));
         Jmsg(jcr, M_ERROR, 0, "%s", mdb->errmsg);
      } else if (num_rows == 1) {
         if ((row = sql_fetch_row(mdb)) == NULL) {
            Mmsg1(mdb->errmsg, _("error fetching row: %s\n"), sql_strerror(mdb));
            Jmsg(jcr, M_ERROR, 0, "%s", mdb->errmsg);
         } else {
            pdbr->PoolId = str_to_int64(row[0]);
            bstrncpy(pdbr->Name, row[1]!=NULL?row[1]:"", sizeof(pdbr->Name));
            pdbr->NumVols = str_to_int64(row[2]);
            pdbr->MaxVols = str_to_int64(row[3]);
            pdbr->UseOnce = str_to_int64(row[4]);
            pdbr->UseCatalog = str_to_int64(row[5]);
            pdbr->AcceptAnyVolume = str_to_int64(row[6]);
            pdbr->AutoPrune = str_to_int64(row[7]);
            pdbr->Recycle = str_to_int64(row[8]);
            pdbr->VolRetention = str_to_int64(row[9]);
            pdbr->VolUseDuration = str_to_int64(row[10]);
            pdbr->MaxVolJobs = str_to_int64(row[11]);
            pdbr->MaxVolFiles = str_to_int64(row[12]);
            pdbr->MaxVolBytes = str_to_uint64(row[13]);
            bstrncpy(pdbr->PoolType, row[14]!=NULL?row[14]:"", sizeof(pdbr->PoolType));
            pdbr->LabelType = str_to_int64(row[15]);
            bstrncpy(pdbr->LabelFormat, row[16]!=NULL?row[16]:"", sizeof(pdbr->LabelFormat));
            pdbr->RecyclePoolId = str_to_int64(row[17]);
            pdbr->ScratchPoolId = str_to_int64(row[18]);
            pdbr->ActionOnPurge = str_to_int32(row[19]);
            ok = true;
         }
      }
      sql_free_result(mdb);
   }
   db_unlock(mdb);
   return ok;
}
/**
 * Get Pool numvols
 * If the PoolId is non-zero, we get its record,
 *  otherwise, we search on the PoolName and we compute the number of volumes
 *
 * Returns: false on failure
 *          true on success
 */
bool db_get_pool_numvols(JCR *jcr, B_DB *mdb, POOL_DBR *pdbr)
{
   bool ok;
   char ed1[50];

   ok = db_get_pool_record(jcr, mdb, pdbr);

   db_lock(mdb);
   if (ok) {
      uint32_t NumVols;
      Mmsg(mdb->cmd, "SELECT count(*) from Media WHERE PoolId=%s",
         edit_int64(pdbr->PoolId, ed1));
      NumVols = get_sql_record_max(jcr, mdb);
      Dmsg2(400, "Actual NumVols=%d Pool NumVols=%d\n", NumVols, pdbr->NumVols);
      if (NumVols != pdbr->NumVols) {
         pdbr->NumVols = NumVols;
         db_update_pool_record(jcr, mdb, pdbr);
      }
   } else {
      Mmsg(mdb->errmsg, _("Pool record not found in Catalog.\n"));
   }
   db_unlock(mdb);
   return ok;
}

/**
 * Get Client Record
 * If the ClientId is non-zero, we get its record,
 *  otherwise, we search on the Client Name
 *
 * Returns: 0 on failure
 *          1 on success
 */
int db_get_client_record(JCR *jcr, B_DB *mdb, CLIENT_DBR *cdbr)
{
   SQL_ROW row;
   int stat = 0;
   char ed1[50];
   int num_rows;
   char esc[MAX_ESCAPE_NAME_LENGTH];

   db_lock(mdb);
   if (cdbr->ClientId != 0) {               /* find by id */
      Mmsg(mdb->cmd,
"SELECT ClientId,Name,Uname,AutoPrune,FileRetention,JobRetention "
"FROM Client WHERE Client.ClientId=%s",
        edit_int64(cdbr->ClientId, ed1));
   } else {                           /* find by name */
      mdb->db_escape_string(jcr, esc, cdbr->Name, strlen(cdbr->Name));
      Mmsg(mdb->cmd,
"SELECT ClientId,Name,Uname,AutoPrune,FileRetention,JobRetention "
"FROM Client WHERE Client.Name='%s'", esc);
   }

   if (QUERY_DB(jcr, mdb, mdb->cmd)) {
      num_rows = sql_num_rows(mdb);
      if (num_rows > 1) {
         Mmsg1(mdb->errmsg, _("More than one Client!: %s\n"),
            edit_uint64(num_rows, ed1));
         Jmsg(jcr, M_ERROR, 0, "%s", mdb->errmsg);
      } else if (num_rows == 1) {
         if ((row = sql_fetch_row(mdb)) == NULL) {
            Mmsg1(mdb->errmsg, _("error fetching row: %s\n"), sql_strerror(mdb));
            Jmsg(jcr, M_ERROR, 0, "%s", mdb->errmsg);
         } else {
            cdbr->ClientId = str_to_int64(row[0]);
            bstrncpy(cdbr->Name, row[1]!=NULL?row[1]:"", sizeof(cdbr->Name));
            bstrncpy(cdbr->Uname, row[2]!=NULL?row[2]:"", sizeof(cdbr->Uname));
            cdbr->AutoPrune = str_to_int64(row[3]);
            cdbr->FileRetention = str_to_int64(row[4]);
            cdbr->JobRetention = str_to_int64(row[5]);
            stat = 1;
         }
      } else {
         Mmsg(mdb->errmsg, _("Client record not found in Catalog.\n"));
      }
      sql_free_result(mdb);
   } else {
      Mmsg(mdb->errmsg, _("Client record not found in Catalog.\n"));
   }
   db_unlock(mdb);
   return stat;
}

/**
 * Get Counter Record
 *
 * Returns: 0 on failure
 *          1 on success
 */
int db_get_counter_record(JCR *jcr, B_DB *mdb, COUNTER_DBR *cr)
{
   SQL_ROW row;
   int num_rows;
   char esc[MAX_ESCAPE_NAME_LENGTH];

   db_lock(mdb);
   mdb->db_escape_string(jcr, esc, cr->Counter, strlen(cr->Counter));

   Mmsg(mdb->cmd, select_counter_values[mdb->db_get_type_index()], esc);
   if (QUERY_DB(jcr, mdb, mdb->cmd)) {
      num_rows = sql_num_rows(mdb);

      /* If more than one, report error, but return first row */
      if (num_rows > 1) {
         Mmsg1(mdb->errmsg, _("More than one Counter!: %d\n"), num_rows);
         Jmsg(jcr, M_ERROR, 0, "%s", mdb->errmsg);
      }
      if (num_rows >= 1) {
         if ((row = sql_fetch_row(mdb)) == NULL) {
            Mmsg1(mdb->errmsg, _("error fetching Counter row: %s\n"), sql_strerror(mdb));
            Jmsg(jcr, M_ERROR, 0, "%s", mdb->errmsg);
            sql_free_result(mdb);
            db_unlock(mdb);
            return 0;
         }
         cr->MinValue = str_to_int64(row[0]);
         cr->MaxValue = str_to_int64(row[1]);
         cr->CurrentValue = str_to_int64(row[2]);
         if (row[3]) {
            bstrncpy(cr->WrapCounter, row[3], sizeof(cr->WrapCounter));
         } else {
            cr->WrapCounter[0] = 0;
         }
         sql_free_result(mdb);
         db_unlock(mdb);
         return 1;
      }
      sql_free_result(mdb);
   } else {
      Mmsg(mdb->errmsg, _("Counter record: %s not found in Catalog.\n"), cr->Counter);
   }
   db_unlock(mdb);
   return 0;
}


/**
 * Get FileSet Record
 * If the FileSetId is non-zero, we get its record,
 *  otherwise, we search on the name
 *
 * Returns: 0 on failure
 *          id on success
 */
int db_get_fileset_record(JCR *jcr, B_DB *mdb, FILESET_DBR *fsr)
{
   SQL_ROW row;
   int stat = 0;
   char ed1[50];
   int num_rows;
   char esc[MAX_ESCAPE_NAME_LENGTH];

   db_lock(mdb);
   if (fsr->FileSetId != 0) {               /* find by id */
      Mmsg(mdb->cmd,
           "SELECT FileSetId,FileSet,MD5,CreateTime FROM FileSet "
           "WHERE FileSetId=%s",
           edit_int64(fsr->FileSetId, ed1));
   } else {                           /* find by name */
      mdb->db_escape_string(jcr, esc, fsr->FileSet, strlen(fsr->FileSet));
      Mmsg(mdb->cmd,
           "SELECT FileSetId,FileSet,MD5,CreateTime FROM FileSet "
           "WHERE FileSet='%s' ORDER BY CreateTime DESC LIMIT 1", esc);
   }

   if (QUERY_DB(jcr, mdb, mdb->cmd)) {
      num_rows = sql_num_rows(mdb);
      if (num_rows > 1) {
         char ed1[30];
         Mmsg1(mdb->errmsg, _("Error got %s FileSets but expected only one!\n"),
            edit_uint64(num_rows, ed1));
         sql_data_seek(mdb, num_rows-1);
      }
      if ((row = sql_fetch_row(mdb)) == NULL) {
         Mmsg1(mdb->errmsg, _("FileSet record \"%s\" not found.\n"), fsr->FileSet);
      } else {
         fsr->FileSetId = str_to_int64(row[0]);
         bstrncpy(fsr->FileSet, row[1]!=NULL?row[1]:"", sizeof(fsr->FileSet));
         bstrncpy(fsr->MD5, row[2]!=NULL?row[2]:"", sizeof(fsr->MD5));
         bstrncpy(fsr->cCreateTime, row[3]!=NULL?row[3]:"", sizeof(fsr->cCreateTime));
         stat = fsr->FileSetId;
      }
      sql_free_result(mdb);
   } else {
      Mmsg(mdb->errmsg, _("FileSet record not found in Catalog.\n"));
   }
   db_unlock(mdb);
   return stat;
}


/**
 * Get the number of Media records
 *
 * Returns: -1 on failure
 *          number on success
 */
int db_get_num_media_records(JCR *jcr, B_DB *mdb)
{
   int stat = 0;

   db_lock(mdb);
   Mmsg(mdb->cmd, "SELECT count(*) from Media");
   stat = get_sql_record_max(jcr, mdb);
   db_unlock(mdb);
   return stat;
}

/**
 * This function returns a list of all the Media record ids for
 *     the current Pool, the correct Media Type, Recyle, Enabled, StorageId, VolBytes
 *     VolumeName if specified
 *  The caller must free ids if non-NULL.
 *
 *  Returns false: on failure
 *          true:  on success
 */
bool db_get_media_ids(JCR *jcr, B_DB *mdb, MEDIA_DBR *mr, int *num_ids, uint32_t *ids[])
{
   SQL_ROW row;
   int i = 0;
   uint32_t *id;
   char ed1[50];
   bool ok = false;
   char buf[MAX_NAME_LENGTH*3]; /* Can contain MAX_NAME_LENGTH*2+1 + AND ....='' */
   char esc[MAX_NAME_LENGTH*2+1];

   db_lock(mdb);
   *ids = NULL;

   Mmsg(mdb->cmd, "SELECT DISTINCT MediaId FROM Media WHERE Recycle=%d AND Enabled=%d ",
        mr->Recycle, mr->Enabled);

   if (*mr->MediaType) {
      db_escape_string(jcr, mdb, esc, mr->MediaType, strlen(mr->MediaType));
      bsnprintf(buf, sizeof(buf), "AND MediaType='%s' ", esc);
      pm_strcat(mdb->cmd, buf);
   }

   if (mr->StorageId) {
      bsnprintf(buf, sizeof(buf), "AND StorageId=%s ", edit_uint64(mr->StorageId, ed1));
      pm_strcat(mdb->cmd, buf);
   }

   if (mr->PoolId) {
      bsnprintf(buf, sizeof(buf), "AND PoolId=%s ", edit_uint64(mr->PoolId, ed1));
      pm_strcat(mdb->cmd, buf);
   }

   if (mr->VolBytes) {
      bsnprintf(buf, sizeof(buf), "AND VolBytes > %s ", edit_uint64(mr->VolBytes, ed1));
      pm_strcat(mdb->cmd, buf);
   }

   if (*mr->VolumeName) {
      db_escape_string(jcr, mdb, esc, mr->VolumeName, strlen(mr->VolumeName));
      bsnprintf(buf, sizeof(buf), "AND VolumeName = '%s' ", esc);
      pm_strcat(mdb->cmd, buf);
   }

   if (*mr->VolStatus) {
      db_escape_string(jcr, mdb, esc, mr->VolStatus, strlen(mr->VolStatus));
      bsnprintf(buf, sizeof(buf), "AND VolStatus = '%s' ", esc);
      pm_strcat(mdb->cmd, buf);
   }

   Dmsg1(100, "q=%s\n", mdb->cmd);

   if (QUERY_DB(jcr, mdb, mdb->cmd)) {
      *num_ids = sql_num_rows(mdb);
      if (*num_ids > 0) {
         id = (uint32_t *)malloc(*num_ids * sizeof(uint32_t));
         while ((row = sql_fetch_row(mdb)) != NULL) {
            id[i++] = str_to_uint64(row[0]);
         }
         *ids = id;
      }
      sql_free_result(mdb);
      ok = true;
   } else {
      Mmsg(mdb->errmsg, _("Media id select failed: ERR=%s\n"), sql_strerror(mdb));
      Jmsg(jcr, M_ERROR, 0, "%s", mdb->errmsg);
      ok = false;
   }
   db_unlock(mdb);
   return ok;
}


/**
 * This function returns a list of all the DBIds that are returned
 *   for the query.
 *
 *  Returns false: on failure
 *          true:  on success
 */
bool db_get_query_dbids(JCR *jcr, B_DB *mdb, POOL_MEM &query, dbid_list &ids)
{
   SQL_ROW row;
   int i = 0;
   bool ok = false;

   db_lock(mdb);
   ids.num_ids = 0;
   if (QUERY_DB(jcr, mdb, query.c_str())) {
      ids.num_ids = sql_num_rows(mdb);
      if (ids.num_ids > 0) {
         if (ids.max_ids < ids.num_ids) {
            free(ids.DBId);
            ids.DBId = (DBId_t *)malloc(ids.num_ids * sizeof(DBId_t));
         }
         while ((row = sql_fetch_row(mdb)) != NULL) {
            ids.DBId[i++] = str_to_uint64(row[0]);
         }
      }
      sql_free_result(mdb);
      ok = true;
   } else {
      Mmsg(mdb->errmsg, _("query dbids failed: ERR=%s\n"), sql_strerror(mdb));
      Jmsg(jcr, M_ERROR, 0, "%s", mdb->errmsg);
      ok = false;
   }
   db_unlock(mdb);
   return ok;
}

/**
 * Get Media Record
 *
 * Returns: false: on failure
 *          true:  on success
 */
bool db_get_media_record(JCR *jcr, B_DB *mdb, MEDIA_DBR *mr)
{
   SQL_ROW row;
   char ed1[50];
   bool ok = false;
   int num_rows;
   char esc[MAX_ESCAPE_NAME_LENGTH];

   db_lock(mdb);
   if (mr->MediaId == 0 && mr->VolumeName[0] == 0) {
      Mmsg(mdb->cmd, "SELECT count(*) from Media");
      mr->MediaId = get_sql_record_max(jcr, mdb);
      db_unlock(mdb);
      return true;
   }
   if (mr->MediaId != 0) {               /* find by id */
      Mmsg(mdb->cmd, "SELECT MediaId,VolumeName,VolJobs,VolFiles,"
         "VolBlocks,VolBytes,VolMounts,"
         "VolErrors,VolWrites,MaxVolBytes,VolCapacityBytes,"
         "MediaType,VolStatus,PoolId,VolRetention,VolUseDuration,MaxVolJobs,"
         "MaxVolFiles,Recycle,Slot,FirstWritten,LastWritten,InChanger,"
         "EndFile,EndBlock,VolParts,LabelType,LabelDate,StorageId,"
         "Enabled,LocationId,RecycleCount,InitialWrite,"
         "ScratchPoolId,RecyclePoolId,VolReadTime,VolWriteTime,ActionOnPurge "
         "FROM Media WHERE MediaId=%s",
         edit_int64(mr->MediaId, ed1));
   } else {                           /* find by name */
      mdb->db_escape_string(jcr, esc, mr->VolumeName, strlen(mr->VolumeName));
      Mmsg(mdb->cmd, "SELECT MediaId,VolumeName,VolJobs,VolFiles,"
         "VolBlocks,VolBytes,VolMounts,"
         "VolErrors,VolWrites,MaxVolBytes,VolCapacityBytes,"
         "MediaType,VolStatus,PoolId,VolRetention,VolUseDuration,MaxVolJobs,"
         "MaxVolFiles,Recycle,Slot,FirstWritten,LastWritten,InChanger,"
         "EndFile,EndBlock,VolParts,LabelType,LabelDate,StorageId,"
         "Enabled,LocationId,RecycleCount,InitialWrite,"
         "ScratchPoolId,RecyclePoolId,VolReadTime,VolWriteTime,ActionOnPurge "
         "FROM Media WHERE VolumeName='%s'", esc);
   }

   if (QUERY_DB(jcr, mdb, mdb->cmd)) {
      char ed1[50];
      num_rows = sql_num_rows(mdb);
      if (num_rows > 1) {
         Mmsg1(mdb->errmsg, _("More than one Volume!: %s\n"),
            edit_uint64(num_rows, ed1));
         Jmsg(jcr, M_ERROR, 0, "%s", mdb->errmsg);
      } else if (num_rows == 1) {
         if ((row = sql_fetch_row(mdb)) == NULL) {
            Mmsg1(mdb->errmsg, _("error fetching row: %s\n"), sql_strerror(mdb));
            Jmsg(jcr, M_ERROR, 0, "%s", mdb->errmsg);
         } else {
            /* return values */
            mr->MediaId = str_to_int64(row[0]);
            bstrncpy(mr->VolumeName, row[1]!=NULL?row[1]:"", sizeof(mr->VolumeName));
            mr->VolJobs = str_to_int64(row[2]);
            mr->VolFiles = str_to_int64(row[3]);
            mr->VolBlocks = str_to_int64(row[4]);
            mr->VolBytes = str_to_uint64(row[5]);
            mr->VolMounts = str_to_int64(row[6]);
            mr->VolErrors = str_to_int64(row[7]);
            mr->VolWrites = str_to_int64(row[8]);
            mr->MaxVolBytes = str_to_uint64(row[9]);
            mr->VolCapacityBytes = str_to_uint64(row[10]);
            bstrncpy(mr->MediaType, row[11]!=NULL?row[11]:"", sizeof(mr->MediaType));
            bstrncpy(mr->VolStatus, row[12]!=NULL?row[12]:"", sizeof(mr->VolStatus));
            mr->PoolId = str_to_int64(row[13]);
            mr->VolRetention = str_to_uint64(row[14]);
            mr->VolUseDuration = str_to_uint64(row[15]);
            mr->MaxVolJobs = str_to_int64(row[16]);
            mr->MaxVolFiles = str_to_int64(row[17]);
            mr->Recycle = str_to_int64(row[18]);
            mr->Slot = str_to_int64(row[19]);
            bstrncpy(mr->cFirstWritten, row[20]!=NULL?row[20]:"", sizeof(mr->cFirstWritten));
            mr->FirstWritten = (time_t)str_to_utime(mr->cFirstWritten);
            bstrncpy(mr->cLastWritten, row[21]!=NULL?row[21]:"", sizeof(mr->cLastWritten));
            mr->LastWritten = (time_t)str_to_utime(mr->cLastWritten);
            mr->InChanger = str_to_uint64(row[22]);
            mr->EndFile = str_to_uint64(row[23]);
            mr->EndBlock = str_to_uint64(row[24]);
            mr->VolParts = str_to_int64(row[25]);
            mr->LabelType = str_to_int64(row[26]);
            bstrncpy(mr->cLabelDate, row[27]!=NULL?row[27]:"", sizeof(mr->cLabelDate));
            mr->LabelDate = (time_t)str_to_utime(mr->cLabelDate);
            mr->StorageId = str_to_int64(row[28]);
            mr->Enabled = str_to_int64(row[29]);
            mr->LocationId = str_to_int64(row[30]);
            mr->RecycleCount = str_to_int64(row[31]);
            bstrncpy(mr->cInitialWrite, row[32]!=NULL?row[32]:"", sizeof(mr->cInitialWrite));
            mr->InitialWrite = (time_t)str_to_utime(mr->cInitialWrite);
            mr->ScratchPoolId = str_to_int64(row[33]);
            mr->RecyclePoolId = str_to_int64(row[34]);
            mr->VolReadTime = str_to_int64(row[35]);
            mr->VolWriteTime = str_to_int64(row[36]);
            mr->ActionOnPurge = str_to_int32(row[37]);

            ok = true;
         }
      } else {
         if (mr->MediaId != 0) {
            Mmsg1(mdb->errmsg, _("Media record with MediaId=%s not found.\n"),
               edit_int64(mr->MediaId, ed1));
         } else {
            Mmsg1(mdb->errmsg, _("Media record for Volume name \"%s\" not found.\n"),
                  mr->VolumeName);
         }
      }
      sql_free_result(mdb);
   } else {
      if (mr->MediaId != 0) {
         Mmsg(mdb->errmsg, _("Media record for MediaId=%u not found in Catalog.\n"),
            mr->MediaId);
       } else {
         Mmsg(mdb->errmsg, _("Media record for Volume Name \"%s\" not found in Catalog.\n"),
            mr->VolumeName);
   }   }
   db_unlock(mdb);
   return ok;
}

/* Remove all MD5 from a query (can save lot of memory with many files) */
static void strip_md5(char *q)
{
   char *p = q;
   while ((p = strstr(p, ", MD5"))) {
      memset(p, ' ', 5 * sizeof(char));
   }
}

/**
 * Find the last "accurate" backup state (that can take deleted files in
 * account)
 * 1) Get all files with jobid in list (F subquery)
 *    Get all files in BaseFiles with jobid in list
 * 2) Take only the last version of each file (Temp subquery) => accurate list
 *    is ok
 * 3) Join the result to file table to get fileindex, jobid and lstat information
 *
 * TODO: See if we can do the SORT only if needed (as an argument)
 */
bool db_get_file_list(JCR *jcr, B_DB *mdb, char *jobids,
                      bool use_md5, bool use_delta,
                      DB_RESULT_HANDLER *result_handler, void *ctx)
{
   if (!*jobids) {
      db_lock(mdb);
      Mmsg(mdb->errmsg, _("ERR=JobIds are empty\n"));
      db_unlock(mdb);
      return false;
   }
   POOL_MEM buf(PM_MESSAGE);
   POOL_MEM buf2(PM_MESSAGE);
   if (use_delta) {
      Mmsg(buf2, select_recent_version_with_basejob_and_delta[db_get_type_index(mdb)],
           jobids, jobids, jobids, jobids);

   } else {
      Mmsg(buf2, select_recent_version_with_basejob[db_get_type_index(mdb)],
           jobids, jobids, jobids, jobids);
   }

   /* bsr code is optimized for JobId sorted, with Delta, we need to get
    * them ordered by date. JobTDate and JobId can be mixed if using Copy
    * or Migration
    */
   Mmsg(buf,
"SELECT Path.Path, Filename.Name, T1.FileIndex, T1.JobId, LStat, DeltaSeq, MD5 "
 "FROM ( %s ) AS T1 "
 "JOIN Filename ON (Filename.FilenameId = T1.FilenameId) "
 "JOIN Path ON (Path.PathId = T1.PathId) "
"WHERE FileIndex > 0 "
"ORDER BY T1.JobTDate, FileIndex ASC",/* Return sorted by JobTDate */
                                      /* FileIndex for restore code */
        buf2.c_str());

   if (!use_md5) {
      strip_md5(buf.c_str());
   }

   Dmsg1(100, "q=%s\n", buf.c_str());

   return db_big_sql_query(mdb, buf.c_str(), result_handler, ctx);
}

/**
 * This procedure gets the base jobid list used by jobids,
 */
bool db_get_used_base_jobids(JCR *jcr, B_DB *mdb,
                             POOLMEM *jobids, db_list_ctx *result)
{
   POOL_MEM buf;
   Mmsg(buf,
 "SELECT DISTINCT BaseJobId "
 "  FROM Job JOIN BaseFiles USING (JobId) "
 " WHERE Job.HasBase = 1 "
 "   AND Job.JobId IN (%s) ", jobids);
   return db_sql_query(mdb, buf.c_str(), db_list_handler, result);
}

/**
 * The decision do change an incr/diff was done before
 * Full : do nothing
 * Differential : get the last full id
 * Incremental : get the last full + last diff + last incr(s) ids
 *
 * If you specify jr->StartTime, it will be used to limit the search
 * in the time. (usually now)
 *
 * TODO: look and merge from ua_restore.c
 */
bool db_accurate_get_jobids(JCR *jcr, B_DB *mdb,
                            JOB_DBR *jr, db_list_ctx *jobids)
{
   bool ret=false;
   char clientid[50], jobid[50], filesetid[50];
   char date[MAX_TIME_LENGTH];
   POOL_MEM query(PM_FNAME);

   /* Take the current time as upper limit if nothing else specified */
   utime_t StartTime = (jr->StartTime)?jr->StartTime:time(NULL);

   bstrutime(date, sizeof(date),  StartTime + 1);
   jobids->reset();

   /* First, find the last good Full backup for this job/client/fileset */
   Mmsg(query, create_temp_accurate_jobids[db_get_type_index(mdb)],
        edit_uint64(jcr->JobId, jobid),
        edit_uint64(jr->ClientId, clientid),
        date,
        edit_uint64(jr->FileSetId, filesetid));

   if (!db_sql_query(mdb, query.c_str(), NULL, NULL)) {
      goto bail_out;
   }

   if (jr->JobLevel == L_INCREMENTAL || jr->JobLevel == L_VIRTUAL_FULL) {
      /* Now, find the last differential backup after the last full */
      Mmsg(query,
"INSERT INTO btemp3%s (JobId, StartTime, EndTime, JobTDate, PurgedFiles) "
 "SELECT JobId, StartTime, EndTime, JobTDate, PurgedFiles "
   "FROM Job JOIN FileSet USING (FileSetId) "
  "WHERE ClientId = %s "
    "AND Level='D' AND JobStatus IN ('T','W') AND Type='B' "
    "AND StartTime > (SELECT EndTime FROM btemp3%s ORDER BY EndTime DESC LIMIT 1) "
    "AND StartTime < '%s' "
    "AND FileSet.FileSet= (SELECT FileSet FROM FileSet WHERE FileSetId = %s) "
  "ORDER BY Job.JobTDate DESC LIMIT 1 ",
           jobid,
           clientid,
           jobid,
           date,
           filesetid);

      if (!db_sql_query(mdb, query.c_str(), NULL, NULL)) {
         goto bail_out;
      }

      /* We just have to take all incremental after the last Full/Diff */
      Mmsg(query,
"INSERT INTO btemp3%s (JobId, StartTime, EndTime, JobTDate, PurgedFiles) "
 "SELECT JobId, StartTime, EndTime, JobTDate, PurgedFiles "
   "FROM Job JOIN FileSet USING (FileSetId) "
  "WHERE ClientId = %s "
    "AND Level='I' AND JobStatus IN ('T','W') AND Type='B' "
    "AND StartTime > (SELECT EndTime FROM btemp3%s ORDER BY EndTime DESC LIMIT 1) "
    "AND StartTime < '%s' "
    "AND FileSet.FileSet= (SELECT FileSet FROM FileSet WHERE FileSetId = %s) "
  "ORDER BY Job.JobTDate DESC ",
           jobid,
           clientid,
           jobid,
           date,
           filesetid);
      if (!db_sql_query(mdb, query.c_str(), NULL, NULL)) {
         goto bail_out;
      }
   }

   /* build a jobid list ie: 1,2,3,4 */
   Mmsg(query, "SELECT JobId FROM btemp3%s ORDER by JobTDate", jobid);
   db_sql_query(mdb, query.c_str(), db_list_handler, jobids);
   Dmsg1(1, "db_accurate_get_jobids=%s\n", jobids->list);
   ret = true;

bail_out:
   Mmsg(query, "DROP TABLE btemp3%s", jobid);
   db_sql_query(mdb, query.c_str(), NULL, NULL);

   return ret;
}

bool db_get_base_file_list(JCR *jcr, B_DB *mdb, bool use_md5,
                           DB_RESULT_HANDLER *result_handler, void *ctx)
{
   POOL_MEM buf(PM_MESSAGE);

   Mmsg(buf,
 "SELECT Path, Name, FileIndex, JobId, LStat, 0 As DeltaSeq, MD5 "
   "FROM new_basefile%lld ORDER BY JobId, FileIndex ASC",
        (uint64_t) jcr->JobId);

   if (!use_md5) {
      strip_md5(buf.c_str());
   }
   return db_sql_query(mdb, buf.c_str(), result_handler, ctx);
}

bool db_get_base_jobid(JCR *jcr, B_DB *mdb, JOB_DBR *jr, JobId_t *jobid)
{
   POOL_MEM query(PM_FNAME);
   utime_t StartTime;
   db_int64_ctx lctx;
   char date[MAX_TIME_LENGTH];
   char esc[MAX_ESCAPE_NAME_LENGTH];
   bool ret=false;
// char clientid[50], filesetid[50];
   *jobid = 0;
   lctx.count = 0;
   lctx.value = 0;

   StartTime = (jr->StartTime)?jr->StartTime:time(NULL);
   bstrutime(date, sizeof(date),  StartTime + 1);
   mdb->db_escape_string(jcr, esc, jr->Name, strlen(jr->Name));

   /* we can take also client name, fileset, etc... */

   Mmsg(query,
 "SELECT JobId, Job, StartTime, EndTime, JobTDate, PurgedFiles "
   "FROM Job "
// "JOIN FileSet USING (FileSetId) JOIN Client USING (ClientId) "
  "WHERE Job.Name = '%s' "
    "AND Level='B' AND JobStatus IN ('T','W') AND Type='B' "
//    "AND FileSet.FileSet= '%s' "
//    "AND Client.Name = '%s' "
    "AND StartTime<'%s' "
  "ORDER BY Job.JobTDate DESC LIMIT 1",
        esc,
//      edit_uint64(jr->ClientId, clientid),
//      edit_uint64(jr->FileSetId, filesetid));
        date);

   Dmsg1(10, "db_get_base_jobid q=%s\n", query.c_str());
   if (!db_sql_query(mdb, query.c_str(), db_int64_handler, &lctx)) {
      goto bail_out;
   }
   *jobid = (JobId_t) lctx.value;

   Dmsg1(10, "db_get_base_jobid=%lld\n", *jobid);
   ret = true;

bail_out:
   return ret;
}

/* Get JobIds associated with a volume */
bool db_get_volume_jobids(JCR *jcr, B_DB *mdb,
                         MEDIA_DBR *mr, db_list_ctx *lst)
{
   char ed1[50];
   bool ret=false;

   db_lock(mdb);
   Mmsg(mdb->cmd, "SELECT DISTINCT JobId FROM JobMedia WHERE MediaId=%s",
        edit_int64(mr->MediaId, ed1));
   ret = db_sql_query(mdb, mdb->cmd, db_list_handler, lst);
   db_unlock(mdb);
   return ret;
}

#endif /* HAVE_SQLITE3 || HAVE_MYSQL || HAVE_POSTGRESQL */
