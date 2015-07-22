/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2015      Bareos GmbH & Co. KG

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
/* originally was Kern Sibbald, MM  separated from util.c MMIII */
/*
 * extracted the TEST_PROGRAM functionality from the files in ..
 * and adapted for unittest framework cmocka
 *
 * Philipp Storz, April 2015
 */
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

extern "C" {
#include <cmocka.h>
}

#include "bareos.h"


void test_scan(void **state) {
   (void) state; /* unused */

   char assertbuf[500];
   char buf[100];
   uint32_t val32;
   uint64_t val64;
   uint32_t FirstIndex, LastIndex, StartFile, EndFile, StartBlock, EndBlock;
   char Job[200];
   int cnt;
   char *helloreq= "Hello *UserAgent* calling\n";
   char *hello = "Hello %127s calling\n";
   char *catreq =
      "CatReq Job=NightlySave.2004-06-11_19.11.32 CreateJobMedia FirstIndex=1 LastIndex=114 StartFile=0 EndFile=0 StartBlock=208 EndBlock=2903248";
   static char Create_job_media[] = "CatReq Job=%127s CreateJobMedia "
      "FirstIndex=%u LastIndex=%u StartFile=%u EndFile=%u "
      "StartBlock=%u EndBlock=%u\n";
   static char OK_media[] = "1000 OK VolName=%127s VolJobs=%u VolFiles=%u"
      " VolBlocks=%u VolBytes=%" lld " VolMounts=%u VolErrors=%u VolWrites=%u"
      " MaxVolBytes=%" lld " VolCapacityBytes=%" lld " VolStatus=%20s"
      " Slot=%d MaxVolJobs=%u MaxVolFiles=%u InChanger=%d"
      " VolReadTime=%" lld " VolWriteTime=%" lld;
   char *media =
      "1000 OK VolName=TestVolume001 VolJobs=0 VolFiles=0 VolBlocks=0 VolBytes=1 VolMounts=0 VolErrors=0 VolWrites=0 MaxVolBytes=0 VolCapacityBytes=0 VolStatus=Append Slot=0 MaxVolJobs=0 MaxVolFiles=0 InChanger=1 VolReadTime=0 VolWriteTime=0";

   struct VOLUME_CAT_INFO {
      /* Media info for the current Volume */
      uint32_t VolCatJobs;               /* number of jobs on this Volume */
      uint32_t VolCatFiles;              /* Number of files */
      uint32_t VolCatBlocks;             /* Number of blocks */
      uint64_t VolCatBytes;              /* Number of bytes written */
      uint32_t VolCatMounts;             /* Number of mounts this volume */
      uint32_t VolCatErrors;             /* Number of errors this volume */
      uint32_t VolCatWrites;             /* Number of writes this volume */
      uint32_t VolCatReads;              /* Number of reads this volume */
      uint64_t VolCatRBytes;             /* Number of bytes read */
      uint32_t VolCatRecycles;           /* Number of recycles this volume */
      int32_t  Slot;                     /* Slot in changer */
      bool     InChanger;                /* Set if vol in current magazine */
      uint32_t VolCatMaxJobs;            /* Maximum Jobs to write to volume */
      uint32_t VolCatMaxFiles;           /* Maximum files to write to volume */
      uint64_t VolCatMaxBytes;           /* Max bytes to write to volume */
      uint64_t VolCatCapacityBytes;      /* capacity estimate */
      uint64_t VolReadTime;              /* time spent reading */
      uint64_t VolWriteTime;             /* time spent writing this Volume */
      uint32_t MinBlocksize;             /* Minimum block size */
      uint32_t MaxBlocksize;             /* Maximum block size */
      char VolCatStatus[20];             /* Volume status */
      char VolCatName[MAX_NAME_LENGTH];  /* Desired volume to mount */
   };
   struct VOLUME_CAT_INFO vol;

   bsscanf("Hello_world 123 1234", "%120s %ld %lld", buf, &val32, &val64);
   //printf("%s %d %lld", buf, val32, val64);
   sprintf(assertbuf, "%s %d %lld", buf, val32, val64);
   assert_string_equal("Hello_world 123 1234", assertbuf);


   *Job=0;
   cnt = bsscanf(catreq, Create_job_media, &Job,
         &FirstIndex, &LastIndex, &StartFile, &EndFile,
         &StartBlock, &EndBlock);
   //printf("cnt=%d Job=%s\n", cnt, Job);
   sprintf(assertbuf,"cnt=%d Job=%s\n", cnt, Job);
   assert_string_equal("cnt=7 Job=NightlySave.2004-06-11_19.11.32\n", assertbuf);

   cnt = bsscanf(helloreq, hello, &Job);
   //printf("cnt=%d Agent=%s\n", cnt, Job);
   sprintf(assertbuf, "cnt=%d Agent=%s", cnt, Job);
   assert_string_equal("cnt=1 Agent=*UserAgent*", assertbuf);

   cnt = bsscanf(media, OK_media,
         vol.VolCatName,
         &vol.VolCatJobs, &vol.VolCatFiles,
         &vol.VolCatBlocks, &vol.VolCatBytes,
         &vol.VolCatMounts, &vol.VolCatErrors,
         &vol.VolCatWrites, &vol.VolCatMaxBytes,
         &vol.VolCatCapacityBytes, vol.VolCatStatus,
         &vol.Slot, &vol.VolCatMaxJobs, &vol.VolCatMaxFiles,
         &vol.InChanger, &vol.VolReadTime, &vol.VolWriteTime,
         &vol.MaxBlocksize, &vol.MinBlocksize);
   //printf("cnt=%d Vol=%s\n", cnt, vol.VolCatName);
   sprintf(assertbuf, "cnt=%d Vol=%s", cnt, vol.VolCatName);
   assert_string_equal("cnt=17 Vol=TestVolume001", assertbuf);

}
