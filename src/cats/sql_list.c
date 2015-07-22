/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2009 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2013 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/
/*
 * BAREOS Catalog Database List records interface routines
 *
 * Kern Sibbald, March 2000
 */

#include "bareos.h"

#if HAVE_SQLITE3 || HAVE_MYSQL || HAVE_POSTGRESQL || HAVE_INGRES || HAVE_DBI

#include "cats.h"
#include "bdb_priv.h"
#include "sql_glue.h"

/* -----------------------------------------------------------------------
 *
 *   Generic Routines (or almost generic)
 *
 * -----------------------------------------------------------------------
 */

/*
 * Submit general SQL query
 */
bool db_list_sql_query(JCR *jcr, B_DB *mdb, const char *query, OUTPUT_FORMATTER *sendit,
                       bool verbose, e_list_type type)
{
   bool retval = false;

   db_lock(mdb);
   if (!sql_query(mdb, query, QF_STORE_RESULT)) {
      Mmsg(mdb->errmsg, _("Query failed: %s\n"), sql_strerror(mdb));
      if (verbose) {
         sendit->decoration(mdb->errmsg);
      }
      goto bail_out;
   }

   sendit->object_start("query");
   list_result(jcr, mdb, sendit, type);
   sendit->object_end("query");
   sql_free_result(mdb);
   retval = true;

bail_out:
   db_unlock(mdb);
   return retval;
}

void db_list_pool_records(JCR *jcr, B_DB *mdb, POOL_DBR *pdbr,
                          OUTPUT_FORMATTER *sendit, e_list_type type)
{
   char esc[MAX_ESCAPE_NAME_LENGTH];

   db_lock(mdb);
   mdb->db_escape_string(jcr, esc, pdbr->Name, strlen(pdbr->Name));

   if (type == VERT_LIST) {
      if (pdbr->Name[0] != 0) {
         Mmsg(mdb->cmd, "SELECT PoolId,Name,NumVols,MaxVols,UseOnce,UseCatalog,"
            "AcceptAnyVolume,VolRetention,VolUseDuration,MaxVolJobs,MaxVolBytes,"
            "AutoPrune,Recycle,PoolType,LabelFormat,Enabled,ScratchPoolId,"
            "RecyclePoolId,LabelType "
            " FROM Pool WHERE Name='%s'", esc);
      } else {
         Mmsg(mdb->cmd, "SELECT PoolId,Name,NumVols,MaxVols,UseOnce,UseCatalog,"
            "AcceptAnyVolume,VolRetention,VolUseDuration,MaxVolJobs,MaxVolBytes,"
            "AutoPrune,Recycle,PoolType,LabelFormat,Enabled,ScratchPoolId,"
            "RecyclePoolId,LabelType "
            " FROM Pool ORDER BY PoolId");
      }
   } else {
      if (pdbr->Name[0] != 0) {
         Mmsg(mdb->cmd, "SELECT PoolId,Name,NumVols,MaxVols,PoolType,LabelFormat "
           "FROM Pool WHERE Name='%s'", esc);
      } else {
         Mmsg(mdb->cmd, "SELECT PoolId,Name,NumVols,MaxVols,PoolType,LabelFormat "
           "FROM Pool ORDER BY PoolId");
      }
   }

   if (!QUERY_DB(jcr, mdb, mdb->cmd)) {
      goto bail_out;
   }

   sendit->object_start("pools");
   list_result(jcr, mdb, sendit, type);
   sendit->object_end("pools");

   sql_free_result(mdb);

bail_out:
   db_unlock(mdb);
}

void db_list_client_records(JCR *jcr, B_DB *mdb, OUTPUT_FORMATTER *sendit, e_list_type type)
{
   db_lock(mdb);
   if (type == VERT_LIST) {
      Mmsg(mdb->cmd, "SELECT ClientId,Name,Uname,AutoPrune,FileRetention,"
         "JobRetention "
         "FROM Client ORDER BY ClientId");
   } else {
      Mmsg(mdb->cmd, "SELECT ClientId,Name,FileRetention,JobRetention "
         "FROM Client ORDER BY ClientId");
   }

   if (!QUERY_DB(jcr, mdb, mdb->cmd)) {
      goto bail_out;
   }

   sendit->object_start("clients");
   list_result(jcr, mdb, sendit, type);
   sendit->object_end("clients");

   sql_free_result(mdb);

bail_out:
   db_unlock(mdb);
}

/*
 * If VolumeName is non-zero, list the record for that Volume
 *   otherwise, list the Volumes in the Pool specified by PoolId
 */
void db_list_media_records(JCR *jcr, B_DB *mdb, MEDIA_DBR *mdbr,
                           OUTPUT_FORMATTER *sendit, e_list_type type)
{
   char ed1[50];
   char esc[MAX_ESCAPE_NAME_LENGTH];

   db_lock(mdb);
   mdb->db_escape_string(jcr, esc, mdbr->VolumeName, strlen(mdbr->VolumeName));

   if (type == VERT_LIST) {
      if (mdbr->VolumeName[0] != 0) {
         Mmsg(mdb->cmd, "SELECT MediaId,VolumeName,Slot,PoolId,"
            "MediaType,FirstWritten,LastWritten,LabelDate,VolJobs,"
            "VolFiles,VolBlocks,VolMounts,VolBytes,VolErrors,VolWrites,"
            "VolCapacityBytes,VolStatus,Enabled,Recycle,VolRetention,"
            "VolUseDuration,MaxVolJobs,MaxVolFiles,MaxVolBytes,InChanger,"
            "EndFile,EndBlock,LabelType,StorageId,DeviceId,"
            "LocationId,RecycleCount,InitialWrite,ScratchPoolId,RecyclePoolId, "
            "Comment"
            " FROM Media WHERE Media.VolumeName='%s'", esc);
      } else {
         Mmsg(mdb->cmd, "SELECT MediaId,VolumeName,Slot,PoolId,"
            "MediaType,FirstWritten,LastWritten,LabelDate,VolJobs,"
            "VolFiles,VolBlocks,VolMounts,VolBytes,VolErrors,VolWrites,"
            "VolCapacityBytes,VolStatus,Enabled,Recycle,VolRetention,"
            "VolUseDuration,MaxVolJobs,MaxVolFiles,MaxVolBytes,InChanger,"
            "EndFile,EndBlock,LabelType,StorageId,DeviceId,"
            "LocationId,RecycleCount,InitialWrite,ScratchPoolId,RecyclePoolId, "
            "Comment"
            " FROM Media WHERE Media.PoolId=%s ORDER BY MediaId",
            edit_int64(mdbr->PoolId, ed1));
      }
   } else {
      if (mdbr->VolumeName[0] != 0) {
         Mmsg(mdb->cmd, "SELECT MediaId,VolumeName,VolStatus,Enabled,"
            "VolBytes,VolFiles,VolRetention,Recycle,Slot,InChanger,MediaType,LastWritten "
            "FROM Media WHERE Media.VolumeName='%s'", esc);
      } else {
         Mmsg(mdb->cmd, "SELECT MediaId,VolumeName,VolStatus,Enabled,"
            "VolBytes,VolFiles,VolRetention,Recycle,Slot,InChanger,MediaType,LastWritten "
            "FROM Media WHERE Media.PoolId=%s ORDER BY MediaId",
            edit_int64(mdbr->PoolId, ed1));
      }
   }

   if (!QUERY_DB(jcr, mdb, mdb->cmd)) {
      goto bail_out;
   }

   sendit->object_start("media");
   list_result(jcr, mdb, sendit, type);
   sendit->object_end("media");

   sql_free_result(mdb);

bail_out:
   db_unlock(mdb);
}

void db_list_jobmedia_records(JCR *jcr, B_DB *mdb, uint32_t JobId,
                              OUTPUT_FORMATTER *sendit, e_list_type type)
{
   char ed1[50];

   db_lock(mdb);
   if (type == VERT_LIST) {
      if (JobId > 0) {                   /* do by JobId */
         Mmsg(mdb->cmd, "SELECT JobMediaId,JobId,Media.MediaId,Media.VolumeName,"
            "FirstIndex,LastIndex,StartFile,JobMedia.EndFile,StartBlock,"
            "JobMedia.EndBlock "
            "FROM JobMedia,Media WHERE Media.MediaId=JobMedia.MediaId "
            "AND JobMedia.JobId=%s", edit_int64(JobId, ed1));
      } else {
         Mmsg(mdb->cmd, "SELECT JobMediaId,JobId,Media.MediaId,Media.VolumeName,"
            "FirstIndex,LastIndex,StartFile,JobMedia.EndFile,StartBlock,"
            "JobMedia.EndBlock "
            "FROM JobMedia,Media WHERE Media.MediaId=JobMedia.MediaId");
      }

   } else {
      if (JobId > 0) {                   /* do by JobId */
         Mmsg(mdb->cmd, "SELECT JobId,Media.VolumeName,FirstIndex,LastIndex "
            "FROM JobMedia,Media WHERE Media.MediaId=JobMedia.MediaId "
            "AND JobMedia.JobId=%s", edit_int64(JobId, ed1));
      } else {
         Mmsg(mdb->cmd, "SELECT JobId,Media.VolumeName,FirstIndex,LastIndex "
            "FROM JobMedia,Media WHERE Media.MediaId=JobMedia.MediaId");
      }
   }
   if (!QUERY_DB(jcr, mdb, mdb->cmd)) {
      goto bail_out;
   }

   sendit->object_start("jobmedia");
   list_result(jcr, mdb, sendit, type);
   sendit->object_end("jobmedia");

   sql_free_result(mdb);

bail_out:
   db_unlock(mdb);
}

void db_list_copies_records(JCR *jcr, B_DB *mdb, uint32_t limit, char *JobIds,
                            OUTPUT_FORMATTER *send, e_list_type type)
{
   POOL_MEM str_limit(PM_MESSAGE);
   POOL_MEM str_jobids(PM_MESSAGE);

   if (limit > 0) {
      Mmsg(str_limit, " LIMIT %d", limit);
   }

   if (JobIds && JobIds[0]) {
      Mmsg(str_jobids, " AND (Job.PriorJobId IN (%s) OR Job.JobId IN (%s)) ",
           JobIds, JobIds);
   }

   db_lock(mdb);
   Mmsg(mdb->cmd,
   "SELECT DISTINCT Job.PriorJobId AS JobId, Job.Job, "
                   "Job.JobId AS CopyJobId, Media.MediaType "
     "FROM Job "
     "JOIN JobMedia USING (JobId) "
     "JOIN Media    USING (MediaId) "
    "WHERE Job.Type = '%c' %s ORDER BY Job.PriorJobId DESC %s",
        (char) JT_JOB_COPY, str_jobids.c_str(), str_limit.c_str());

   if (!QUERY_DB(jcr, mdb, mdb->cmd)) {
      goto bail_out;
   }

   if (sql_num_rows(mdb)) {
      if (JobIds && JobIds[0]) {
         send->decoration(_("These JobIds have copies as follows:\n"));
      } else {
         send->decoration(_("The catalog contains copies as follows:\n"));
      }

      send->object_start("copies");
      list_result(jcr, mdb, send, type);
      send->object_end("copies");
   }

   sql_free_result(mdb);

bail_out:
   db_unlock(mdb);
}

void db_list_joblog_records(JCR *jcr, B_DB *mdb, uint32_t JobId,
                            OUTPUT_FORMATTER *sendit, e_list_type type)
{
   char ed1[50];

   if (JobId <= 0) {
      return;
   }
   db_lock(mdb);
   if (type == VERT_LIST) {
      Mmsg(mdb->cmd, "SELECT LogText FROM Log "
                     "WHERE Log.JobId=%s ORDER BY Log.LogId", edit_int64(JobId, ed1));
   } else {
      Mmsg(mdb->cmd, "SELECT LogText FROM Log "
                     "WHERE Log.JobId=%s ORDER BY Log.LogId", edit_int64(JobId, ed1));
      /*
       * When something else then a vertical list is requested set the list type
       * to RAW_LIST e.g. non formated raw data as that makes the only sense for
       * the logtext output. The logtext already has things like \n etc in it
       * so we should just dump the raw content out for the best visible output.
       */
      type = RAW_LIST;
   }
   if (!QUERY_DB(jcr, mdb, mdb->cmd)) {
      goto bail_out;
   }

   sendit->object_start("joblog");
   list_result(jcr, mdb, sendit, type);
   sendit->object_end("joblog");

   sql_free_result(mdb);

bail_out:
   db_unlock(mdb);
}

/*
 * List Job record(s) that match JOB_DBR
 *
 *  Currently, we return all jobs or if jr->JobId is set,
 *  only the job with the specified id.
 */
void db_list_job_records(JCR *jcr, B_DB *mdb, JOB_DBR *jr, OUTPUT_FORMATTER *sendit,
                         e_list_type type)
{
   char ed1[50];
   char limit[100];
   char esc[MAX_ESCAPE_NAME_LENGTH];

   db_lock(mdb);
   if (jr->limit > 0) {
      snprintf(limit, sizeof(limit), " LIMIT %d", jr->limit);
   } else {
      limit[0] = 0;
   }
   if (type == VERT_LIST) {
      if (jr->JobId == 0 && jr->Job[0] == 0) {
         Mmsg(mdb->cmd,
            "SELECT JobId,Job,Job.Name,PurgedFiles,Type,Level,"
            "Job.ClientId,Client.Name as ClientName,JobStatus,SchedTime,"
            "StartTime,EndTime,RealEndTime,JobTDate,"
            "VolSessionId,VolSessionTime,JobFiles,JobErrors,"
            "JobMissingFiles,Job.PoolId,Pool.Name as PooLname,PriorJobId,"
            "Job.FileSetId,FileSet.FileSet "
            "FROM Job,Client,Pool,FileSet WHERE "
            "Client.ClientId=Job.ClientId AND Pool.PoolId=Job.PoolId "
            "AND FileSet.FileSetId=Job.FileSetId  ORDER BY StartTime%s", limit);
      } else {                           /* single record */
         Mmsg(mdb->cmd,
            "SELECT JobId,Job,Job.Name,PurgedFiles,Type,Level,"
            "Job.ClientId,Client.Name,JobStatus,SchedTime,"
            "StartTime,EndTime,RealEndTime,JobTDate,"
            "VolSessionId,VolSessionTime,JobFiles,JobErrors,"
            "JobMissingFiles,Job.PoolId,Pool.Name as PooLname,PriorJobId,"
            "Job.FileSetId,FileSet.FileSet "
            "FROM Job,Client,Pool,FileSet WHERE Job.JobId=%s AND "
            "Client.ClientId=Job.ClientId AND Pool.PoolId=Job.PoolId "
            "AND FileSet.FileSetId=Job.FileSetId",
            edit_int64(jr->JobId, ed1));
      }
   } else {
      if (jr->Name[0] != 0) {
         mdb->db_escape_string(jcr, esc, jr->Name, strlen(jr->Name));
         Mmsg(mdb->cmd,
           "SELECT JobId,Name,StartTime,Type,Level,JobFiles,JobBytes,JobStatus "
             "FROM Job WHERE Name='%s' ORDER BY JobId ASC", esc);
      } else if (jr->Job[0] != 0) {
         mdb->db_escape_string(jcr, esc, jr->Job, strlen(jr->Job));
         Mmsg(mdb->cmd,
            "SELECT JobId,Name,StartTime,Type,Level,JobFiles,JobBytes,JobStatus "
            "FROM Job WHERE Job='%s' ORDER BY JobId ASC", esc);
      } else if (jr->JobId != 0) {
         Mmsg(mdb->cmd,
            "SELECT JobId,Name,StartTime,Type,Level,JobFiles,JobBytes,JobStatus "
            "FROM Job WHERE JobId=%s", edit_int64(jr->JobId, ed1));
      } else {                           /* all records */
         Mmsg(mdb->cmd,
           "SELECT JobId,Name,StartTime,Type,Level,JobFiles,JobBytes,JobStatus "
           "FROM Job ORDER BY JobId ASC%s", limit);
      }
   }
   if (!QUERY_DB(jcr, mdb, mdb->cmd)) {
      goto bail_out;
   }
   sendit->object_start("jobs");
   list_result(jcr, mdb, sendit, type);
   sendit->object_end("jobs");

   sql_free_result(mdb);

bail_out:
   db_unlock(mdb);
}

/*
 * List Job totals
 *
 */
void db_list_job_totals(JCR *jcr, B_DB *mdb, JOB_DBR *jr, OUTPUT_FORMATTER *sendit)
{
   db_lock(mdb);

   /* List by Job */
   Mmsg(mdb->cmd, "SELECT  count(*) AS Jobs,sum(JobFiles) "
      "AS Files,sum(JobBytes) AS Bytes,Name AS Job FROM Job GROUP BY Name");

   if (!QUERY_DB(jcr, mdb, mdb->cmd)) {
      goto bail_out;
   }

   list_result(jcr, mdb, sendit, HORZ_LIST);

   sql_free_result(mdb);

   /* Do Grand Total */
   Mmsg(mdb->cmd, "SELECT count(*) AS Jobs,sum(JobFiles) "
        "AS Files,sum(JobBytes) As Bytes FROM Job");

   if (!QUERY_DB(jcr, mdb, mdb->cmd)) {
      goto bail_out;
   }

   list_result(jcr, mdb, sendit, HORZ_LIST);

   sql_free_result(mdb);

bail_out:
   db_unlock(mdb);
}

void db_list_files_for_job(JCR *jcr, B_DB *mdb, JobId_t jobid, OUTPUT_FORMATTER *sendit)
{
   char ed1[50];
   LIST_CTX lctx(jcr, mdb, sendit, NF_LIST);

   db_lock(mdb);

   /*
    * Stupid MySQL is NON-STANDARD !
    */
   if (db_get_type_index(mdb) == SQL_TYPE_MYSQL) {
      Mmsg(mdb->cmd, "SELECT CONCAT(Path.Path,Filename.Name) AS Filename "
           "FROM (SELECT PathId, FilenameId FROM File WHERE JobId=%s "
                  "UNION ALL "
                 "SELECT PathId, FilenameId "
                   "FROM BaseFiles JOIN File "
                         "ON (BaseFiles.FileId = File.FileId) "
                  "WHERE BaseFiles.JobId = %s"
           ") AS F, Filename,Path "
           "WHERE Filename.FilenameId=F.FilenameId "
           "AND Path.PathId=F.PathId",
           edit_int64(jobid, ed1), ed1);
   } else {
      Mmsg(mdb->cmd, "SELECT Path.Path||Filename.Name AS Filename "
           "FROM (SELECT PathId, FilenameId FROM File WHERE JobId=%s "
                  "UNION ALL "
                 "SELECT PathId, FilenameId "
                   "FROM BaseFiles JOIN File "
                         "ON (BaseFiles.FileId = File.FileId) "
                  "WHERE BaseFiles.JobId = %s"
           ") AS F, Filename,Path "
           "WHERE Filename.FilenameId=F.FilenameId "
           "AND Path.PathId=F.PathId",
           edit_int64(jobid, ed1), ed1);
   }

   sendit->object_start();
   if (!db_big_sql_query(mdb, mdb->cmd, list_result, &lctx)) {
       goto bail_out;
   }
   sendit->object_end();

   sql_free_result(mdb);

bail_out:
   db_unlock(mdb);
}

void db_list_base_files_for_job(JCR *jcr, B_DB *mdb, JobId_t jobid, OUTPUT_FORMATTER *sendit)
{
   char ed1[50];
   LIST_CTX lctx(jcr, mdb, sendit, NF_LIST);

   db_lock(mdb);

   /*
    * Stupid MySQL is NON-STANDARD !
    */
   if (db_get_type_index(mdb) == SQL_TYPE_MYSQL) {
      Mmsg(mdb->cmd, "SELECT CONCAT(Path.Path,Filename.Name) AS Filename "
           "FROM BaseFiles, File, Filename, Path "
           "WHERE BaseFiles.JobId=%s AND BaseFiles.BaseJobId = File.JobId "
           "AND BaseFiles.FileId = File.FileId "
           "AND Filename.FilenameId=File.FilenameId "
           "AND Path.PathId=File.PathId",
         edit_int64(jobid, ed1));
   } else {
      Mmsg(mdb->cmd, "SELECT Path.Path||Filename.Name AS Filename "
           "FROM BaseFiles, File, Filename, Path "
           "WHERE BaseFiles.JobId=%s AND BaseFiles.BaseJobId = File.JobId "
           "AND BaseFiles.FileId = File.FileId "
           "AND Filename.FilenameId=File.FilenameId "
           "AND Path.PathId=File.PathId",
           edit_int64(jobid, ed1));
   }

   sendit->object_start("files");
   if (!db_big_sql_query(mdb, mdb->cmd, list_result, &lctx)) {
       goto bail_out;
   }
   sendit->object_end("files");

   sql_free_result(mdb);

bail_out:
   db_unlock(mdb);
}

/*
 * List fileset
 */
void db_list_filesets(JCR *jcr, B_DB *mdb, JOB_DBR *jr, OUTPUT_FORMATTER *sendit,
                      e_list_type type)
{
   POOL_MEM str_limit(PM_MESSAGE);
   char esc[MAX_ESCAPE_NAME_LENGTH];

   db_lock(mdb);
   if (jr->limit > 0) {
      Mmsg(str_limit, " LIMIT %d", jr->limit);
   }

   if (jr->Name[0] != 0) {
      mdb->db_escape_string(jcr, esc, jr->Name, strlen(jr->Name));
      Mmsg(mdb->cmd, "SELECT DISTINCT FileSetId, FileSet, MD5, CreateTime "
           "FROM Job, FileSet "
           "WHERE Job.FileSetId = FileSet.FileSetId "
           "AND Job.Name='%s'%s", esc, str_limit.c_str());
   } else if (jr->Job[0] != 0) {
      mdb->db_escape_string(jcr, esc, jr->Job, strlen(jr->Job));
      Mmsg(mdb->cmd, "SELECT DISTINCT FileSetId, FileSet, MD5, CreateTime "
           "FROM Job, FileSet "
           "WHERE Job.FileSetId = FileSet.FileSetId "
           "AND Job.Name='%s'%s", esc, str_limit.c_str());
   } else if (jr->JobId != 0) {
      Mmsg(mdb->cmd, "SELECT DISTINCT FileSetId, FileSet, MD5, CreateTime "
           "FROM Job, FileSet "
           "WHERE Job.FileSetId = FileSet.FileSetId "
           "AND Job.JobId='%s'%s", edit_int64(jr->JobId, esc), str_limit.c_str());
   } else {                           /* all records */
      Mmsg(mdb->cmd, "SELECT DISTINCT FileSetId, FileSet, MD5, CreateTime "
           "FROM FileSet ORDER BY FileSetId ASC%s", str_limit.c_str());
   }

   if (!QUERY_DB(jcr, mdb, mdb->cmd)) {
      goto bail_out;
   }
   sendit->object_start("filesets");
   list_result(jcr, mdb, sendit, type);
   sendit->object_end("filesets");

   sql_free_result(mdb);

bail_out:
   db_unlock(mdb);
}

#endif /* HAVE_SQLITE3 || HAVE_MYSQL || HAVE_POSTGRESQL || HAVE_INGRES || HAVE_DBI */
