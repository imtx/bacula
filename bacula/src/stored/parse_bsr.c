/*     
 *   Parse a Bootstrap Records (used for restores) 
 *  
 *     Kern Sibbald, June MMII
 *
 *   Version $Id$
 */

/*
   Copyright (C) 2002 Kern Sibbald and John Walker

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the Free
   MA 02111-1307, USA.

 */


#include "bacula.h"
#include "stored.h"

typedef BSR * (ITEM_HANDLER)(LEX *lc, BSR *bsr);

static BSR *store_vol(LEX *lc, BSR *bsr);
static BSR *store_client(LEX *lc, BSR *bsr);
static BSR *store_job(LEX *lc, BSR *bsr);
static BSR *store_jobid(LEX *lc, BSR *bsr);
static BSR *store_count(LEX *lc, BSR *bsr);
static BSR *store_jobtype(LEX *lc, BSR *bsr);
static BSR *store_joblevel(LEX *lc, BSR *bsr);
static BSR *store_findex(LEX *lc, BSR *bsr);
static BSR *store_sessid(LEX *lc, BSR *bsr);
static BSR *store_volfile(LEX *lc, BSR *bsr);
static BSR *store_volblock(LEX *lc, BSR *bsr);
static BSR *store_sesstime(LEX *lc, BSR *bsr);
static BSR *store_include(LEX *lc, BSR *bsr);
static BSR *store_exclude(LEX *lc, BSR *bsr);
static BSR *store_stream(LEX *lc, BSR *bsr);
static BSR *store_slot(LEX *lc, BSR *bsr);

struct kw_items {
   char *name;
   ITEM_HANDLER *handler;
};

/*
 * List of all keywords permitted in bsr files and their handlers
 */
struct kw_items items[] = {
   {"volume", store_vol},
   {"client", store_client},
   {"job", store_job},
   {"jobid", store_jobid},
   {"count", store_count},
   {"fileindex", store_findex},
   {"jobtype", store_jobtype},
   {"joblevel", store_joblevel},
   {"volsessionid", store_sessid},
   {"volsessiontime", store_sesstime},
   {"include", store_include},
   {"exclude", store_exclude},
   {"volfile", store_volfile},
   {"volblock", store_volblock},
   {"stream",  store_stream},
   {"slot",    store_slot},
   {NULL, NULL}

};

/* 
 * Create a BSR record
 */
static BSR *new_bsr() 
{
   BSR *bsr = (BSR *)malloc(sizeof(BSR));
   memset(bsr, 0, sizeof(BSR));
   return bsr;
}

/*
 * Format a scanner error message 
 */
static void s_err(char *file, int line, LEX *lc, char *msg, ...)
{
   JCR *jcr = (JCR *)(lc->caller_ctx);
   va_list arg_ptr;
   char buf[MAXSTRING];

   va_start(arg_ptr, msg);
   bvsnprintf(buf, sizeof(buf), msg, arg_ptr);
   va_end(arg_ptr);
     
   if (jcr) {
      Jmsg(jcr, M_FATAL, 0, _("Bootstrap file error: %s\n\
            : Line %d, col %d of file %s\n%s\n"),
	 buf, lc->line_no, lc->col_no, lc->fname, lc->line);
   } else {
      e_msg(file, line, M_FATAL, 0, _("Bootstrap file error: %s\n\
            : Line %d, col %d of file %s\n%s\n"),
	 buf, lc->line_no, lc->col_no, lc->fname, lc->line);
   }
}


/*********************************************************************
 *
 *	Parse Bootstrap file
 *
 */
BSR *parse_bsr(JCR *jcr, char *cf)
{
   LEX *lc = NULL;
   int token, i;
   BSR *root_bsr = new_bsr();
   BSR *bsr = root_bsr;

   Dmsg1(200, "Enter parse_bsf %s\n", cf);
   lc = lex_open_file(lc, cf, s_err);
   lc->caller_ctx = (void *)jcr;
   while ((token=lex_get_token(lc, T_ALL)) != T_EOF) {
      Dmsg1(200, "parse got token=%s\n", lex_tok_to_str(token));
      if (token == T_EOL) {
	 continue;
      }
      for (i=0; items[i].name; i++) {
	 if (strcasecmp(items[i].name, lc->str) == 0) {
	    token = lex_get_token(lc, T_ALL);
            Dmsg1 (200, "in T_IDENT got token=%s\n", lex_tok_to_str(token));
	    if (token != T_EQUALS) {
               scan_err1(lc, "expected an equals, got: %s", lc->str);
	       bsr = NULL;
	       break;
	    }
            Dmsg1(200, "calling handler for %s\n", items[i].name);
	    /* Call item handler */
	    bsr = items[i].handler(lc, bsr);
	    i = -1;
	    break;
	 }
      }
      if (i >= 0) {
         Dmsg1(200, "Keyword = %s\n", lc->str);
         scan_err1(lc, "Keyword %s not found", lc->str);
	 bsr = NULL;
	 break;
      }
      if (!bsr) {
	 break;
      }
   }
   lc = lex_close_file(lc);
   Dmsg0(200, "Leave parse_bsf()\n");
   if (!bsr) {
      free_bsr(root_bsr);
      root_bsr = NULL;
   }
   return root_bsr;
}

static BSR *store_vol(LEX *lc, BSR *bsr)
{
   int token;
   BSR_VOLUME *volume;
   char *p, *n;
    
   token = lex_get_token(lc, T_STRING);
   if (token == T_ERROR) {
      return NULL;
   }
   if (bsr->volume) {
      bsr->next = new_bsr();
      bsr = bsr->next;
   }
   /* This may actually be more than one volume separated by a |  
    * If so, separate them.
    */
   for (p=lc->str; p && *p; ) {
      n = strchr(p, '|');
      if (n) {
	 *n++ = 0;
      }
      volume = (BSR_VOLUME *)malloc(sizeof(BSR_VOLUME));
      memset(volume, 0, sizeof(BSR_VOLUME));
      strcpy(volume->VolumeName, p);
      /* Add it to the end of the volume chain */
      if (!bsr->volume) {
	 bsr->volume = volume;
      } else {
	 BSR_VOLUME *bc = bsr->volume;
	 for ( ;bc->next; bc=bc->next)	
	    { }
	 bc->next = volume;
      }
      p = n;
   }
   return bsr;
}

static BSR *store_client(LEX *lc, BSR *bsr)
{
   int token;
   BSR_CLIENT *client;
    
   for (;;) {
      token = lex_get_token(lc, T_NAME);
      if (token == T_ERROR) {
	 return NULL;
      }
      client = (BSR_CLIENT *)malloc(sizeof(BSR_CLIENT));
      memset(client, 0, sizeof(BSR_CLIENT));
      strcpy(client->ClientName, lc->str);
      /* Add it to the end of the client chain */
      if (!bsr->client) {
	 bsr->client = client;
      } else {
	 BSR_CLIENT *bc = bsr->client;
	 for ( ;bc->next; bc=bc->next)	
	    { }
	 bc->next = client;
      }
      token = lex_get_token(lc, T_ALL);
      if (token != T_COMMA) {
	 break;
      }
   }
   return bsr;
}

static BSR *store_job(LEX *lc, BSR *bsr)
{
   int token;
   BSR_JOB *job;
    
   for (;;) {
      token = lex_get_token(lc, T_NAME);
      if (token == T_ERROR) {
	 return NULL;
      }
      job = (BSR_JOB *)malloc(sizeof(BSR_JOB));
      memset(job, 0, sizeof(BSR_JOB));
      strcpy(job->Job, lc->str);
      /* Add it to the end of the client chain */
      if (!bsr->job) {
	 bsr->job = job;
      } else {
	 /* Add to end of chain */
	 BSR_JOB *bc = bsr->job;
	 for ( ;bc->next; bc=bc->next)
	    { }
	 bc->next = job;
      }
      token = lex_get_token(lc, T_ALL);
      if (token != T_COMMA) {
	 break;
      }
   }
   return bsr;
}

static BSR *store_findex(LEX *lc, BSR *bsr)
{
   int token;
   BSR_FINDEX *findex;

   for (;;) {
      token = lex_get_token(lc, T_PINT32_RANGE);
      if (token == T_ERROR) {
	 return NULL;
      }
      findex = (BSR_FINDEX *)malloc(sizeof(BSR_FINDEX));
      memset(findex, 0, sizeof(BSR_FINDEX));
      findex->findex = lc->pint32_val;
      findex->findex2 = lc->pint32_val2;
      /* Add it to the end of the chain */
      if (!bsr->FileIndex) {
	 bsr->FileIndex = findex;
      } else {
	 /* Add to end of chain */
	 BSR_FINDEX *bs = bsr->FileIndex;
	 for ( ;bs->next; bs=bs->next)
	    {  }
	 bs->next = findex;
      }
      token = lex_get_token(lc, T_ALL);
      if (token != T_COMMA) {
	 break;
      }
   }
   return bsr;
}


static BSR *store_jobid(LEX *lc, BSR *bsr)
{
   int token;
   BSR_JOBID *jobid;

   for (;;) {
      token = lex_get_token(lc, T_PINT32_RANGE);
      if (token == T_ERROR) {
	 return NULL;
      }
      jobid = (BSR_JOBID *)malloc(sizeof(BSR_JOBID));
      memset(jobid, 0, sizeof(BSR_JOBID));
      jobid->JobId = lc->pint32_val;
      jobid->JobId2 = lc->pint32_val2;
      /* Add it to the end of the chain */
      if (!bsr->JobId) {
	 bsr->JobId = jobid;
      } else {
	 /* Add to end of chain */
	 BSR_JOBID *bs = bsr->JobId;
	 for ( ;bs->next; bs=bs->next)
	    {  }
	 bs->next = jobid;
      }
      token = lex_get_token(lc, T_ALL);
      if (token != T_COMMA) {
	 break;
      }
   }
   return bsr;
}


static BSR *store_count(LEX *lc, BSR *bsr)
{
   int token;

   token = lex_get_token(lc, T_PINT32);
   if (token == T_ERROR) {
      return NULL;
   }
   bsr->count = lc->pint32_val;
   scan_to_eol(lc);
   return bsr;
}


static BSR *store_jobtype(LEX *lc, BSR *bsr)
{
   /* *****FIXME****** */
   Dmsg0(-1, "JobType not yet implemented\n");
   return bsr;
}


static BSR *store_joblevel(LEX *lc, BSR *bsr)
{
   /* *****FIXME****** */
   Dmsg0(-1, "JobLevel not yet implemented\n");
   return bsr;
}




/*
 * Routine to handle Volume start/end file   
 */
static BSR *store_volfile(LEX *lc, BSR *bsr)
{
   int token;
   BSR_VOLFILE *volfile;

   for (;;) {
      token = lex_get_token(lc, T_PINT32_RANGE);
      if (token == T_ERROR) {
	 return NULL;
      }
      volfile = (BSR_VOLFILE *)malloc(sizeof(BSR_VOLFILE));
      memset(volfile, 0, sizeof(BSR_VOLFILE));
      volfile->sfile = lc->pint32_val;
      volfile->efile = lc->pint32_val2;
      /* Add it to the end of the chain */
      if (!bsr->volfile) {
	 bsr->volfile = volfile;
      } else {
	 /* Add to end of chain */
	 BSR_VOLFILE *bs = bsr->volfile;
	 for ( ;bs->next; bs=bs->next)
	    {  }
	 bs->next = volfile;
      }
      token = lex_get_token(lc, T_ALL);
      if (token != T_COMMA) {
	 break;
      }
   }
   return bsr;
}


/*
 * Routine to handle Volume start/end Block  
 */
static BSR *store_volblock(LEX *lc, BSR *bsr)
{
   int token;
   BSR_VOLBLOCK *volblock;

   for (;;) {
      token = lex_get_token(lc, T_PINT32_RANGE);
      if (token == T_ERROR) {
	 return NULL;
      }
      volblock = (BSR_VOLBLOCK *)malloc(sizeof(BSR_VOLBLOCK));
      memset(volblock, 0, sizeof(BSR_VOLBLOCK));
      volblock->sblock = lc->pint32_val;
      volblock->eblock = lc->pint32_val2;
      /* Add it to the end of the chain */
      if (!bsr->volblock) {
	 bsr->volblock = volblock;
      } else {
	 /* Add to end of chain */
	 BSR_VOLBLOCK *bs = bsr->volblock;
	 for ( ;bs->next; bs=bs->next)
	    {  }
	 bs->next = volblock;
      }
      token = lex_get_token(lc, T_ALL);
      if (token != T_COMMA) {
	 break;
      }
   }
   return bsr;
}


static BSR *store_sessid(LEX *lc, BSR *bsr)
{
   int token;
   BSR_SESSID *sid;

   for (;;) {
      token = lex_get_token(lc, T_PINT32_RANGE);
      if (token == T_ERROR) {
	 return NULL;
      }
      sid = (BSR_SESSID *)malloc(sizeof(BSR_SESSID));
      memset(sid, 0, sizeof(BSR_SESSID));
      sid->sessid = lc->pint32_val;
      sid->sessid2 = lc->pint32_val2;
      /* Add it to the end of the chain */
      if (!bsr->sessid) {
	 bsr->sessid = sid;
      } else {
	 /* Add to end of chain */
	 BSR_SESSID *bs = bsr->sessid;
	 for ( ;bs->next; bs=bs->next)
	    {  }
	 bs->next = sid;
      }
      token = lex_get_token(lc, T_ALL);
      if (token != T_COMMA) {
	 break;
      }
   }
   return bsr;
}

static BSR *store_sesstime(LEX *lc, BSR *bsr)
{
   int token;
   BSR_SESSTIME *stime;

   for (;;) {
      token = lex_get_token(lc, T_PINT32);
      if (token == T_ERROR) {
	 return NULL;
      }
      stime = (BSR_SESSTIME *)malloc(sizeof(BSR_SESSTIME));
      memset(stime, 0, sizeof(BSR_SESSTIME));
      stime->sesstime = lc->pint32_val;
      /* Add it to the end of the chain */
      if (!bsr->sesstime) {
	 bsr->sesstime = stime;
      } else {
	 /* Add to end of chain */
	 BSR_SESSTIME *bs = bsr->sesstime;
	 for ( ;bs->next; bs=bs->next)
	    { }
	 bs->next = stime;
      }
      token = lex_get_token(lc, T_ALL);
      if (token != T_COMMA) {
	 break;
      }
   }
   return bsr;
}


static BSR *store_stream(LEX *lc, BSR *bsr)
{
   int token;
   BSR_STREAM *stream;

   for (;;) {
      token = lex_get_token(lc, T_INT32);
      if (token == T_ERROR) {
	 return NULL;
      }
      stream = (BSR_STREAM *)malloc(sizeof(BSR_STREAM));
      memset(stream, 0, sizeof(BSR_STREAM));
      stream->stream = lc->int32_val;
      /* Add it to the end of the chain */
      if (!bsr->stream) {
	 bsr->stream = stream;
      } else {
	 /* Add to end of chain */
	 BSR_STREAM *bs = bsr->stream;
	 for ( ;bs->next; bs=bs->next)
	    { }
	 bs->next = stream;
      }
      token = lex_get_token(lc, T_ALL);
      if (token != T_COMMA) {
	 break;
      }
   }
   return bsr;
}

static BSR *store_slot(LEX *lc, BSR *bsr)
{
   int token;

   token = lex_get_token(lc, T_PINT32);
   if (token == T_ERROR) {
      return NULL;
   }
   bsr->Slot = lc->pint32_val;
   scan_to_eol(lc);
   return bsr;
}

static BSR *store_include(LEX *lc, BSR *bsr)
{
   scan_to_eol(lc);
   return bsr;
}

static BSR *store_exclude(LEX *lc, BSR *bsr)
{
   scan_to_eol(lc);
   return bsr;
}

void dump_volfile(BSR_VOLFILE *volfile)
{
   if (volfile) {
      Dmsg2(-1, "VolFile     : %u-%u\n", volfile->sfile, volfile->efile);
      dump_volfile(volfile->next);
   }
}

void dump_volblock(BSR_VOLBLOCK *volblock)
{
   if (volblock) {
      Dmsg2(-1, "VolBlock    : %u-%u\n", volblock->sblock, volblock->eblock);
      dump_volblock(volblock->next);
   }
}


void dump_findex(BSR_FINDEX *FileIndex)
{
   if (FileIndex) {
      if (FileIndex->findex == FileIndex->findex2) {
         Dmsg1(-1, "FileIndex   : %u\n", FileIndex->findex);
      } else {
         Dmsg2(-1, "FileIndex   : %u-%u\n", FileIndex->findex, FileIndex->findex2);
      }
      dump_findex(FileIndex->next);
   }
}

void dump_jobid(BSR_JOBID *jobid)
{
   if (jobid) {
      if (jobid->JobId == jobid->JobId2) {
         Dmsg1(-1, "JobId       : %u\n", jobid->JobId);
      } else {
         Dmsg2(-1, "JobId       : %u-%u\n", jobid->JobId, jobid->JobId2);
      }
      dump_jobid(jobid->next);
   }
}

void dump_sessid(BSR_SESSID *sessid)
{
   if (sessid) {
      if (sessid->sessid == sessid->sessid2) {
         Dmsg1(-1, "SessId      : %u\n", sessid->sessid);
      } else {
         Dmsg2(-1, "SessId      : %u-%u\n", sessid->sessid, sessid->sessid2);
      }
      dump_sessid(sessid->next);
   }
}

void dump_volume(BSR_VOLUME *volume)
{
   if (volume) {
      Dmsg1(-1, "VolumeName  : %s\n", volume->VolumeName);
      dump_volume(volume->next);
   }
}


void dump_client(BSR_CLIENT *client)
{
   if (client) {
      Dmsg1(-1, "Client      : %s\n", client->ClientName);
      dump_client(client->next);
   }
}

void dump_job(BSR_JOB *job)
{
   if (job) {
      Dmsg1(-1, "Job          : %s\n", job->Job);
      dump_job(job->next);
   }
}

void dump_sesstime(BSR_SESSTIME *sesstime)
{
   if (sesstime) {
      Dmsg1(-1, "SessTime    : %u\n", sesstime->sesstime);
      dump_sesstime(sesstime->next);
   }
}





void dump_bsr(BSR *bsr)
{
   if (!bsr) {
      Dmsg0(-1, "BSR is NULL\n");
      return;
   }
   Dmsg1(-1,   
"Next        : 0x%x\n", bsr->next);
   dump_volume(bsr->volume);
   dump_sessid(bsr->sessid);
   dump_sesstime(bsr->sesstime);
   dump_volfile(bsr->volfile);
   dump_volblock(bsr->volblock);
   dump_client(bsr->client);
   dump_jobid(bsr->JobId);
   dump_job(bsr->job);
   dump_findex(bsr->FileIndex);
   if (bsr->Slot) {
      Dmsg1(-1, "Slot        : %u\n", bsr->Slot);
   }
   if (bsr->count) {
      Dmsg1(-1, "count       : %u\n", bsr->count);
   }
   if (bsr->next) {
      Dmsg0(-1, "\n");
      dump_bsr(bsr->next);
   }
}



/*********************************************************************
 *
 *	Free bsr resources
 */

static void free_bsr_item(BSR *bsr)
{
   if (bsr) {
      free_bsr_item(bsr->next);
      free(bsr);
   }
}

void free_bsr(BSR *bsr)
{
   if (!bsr) {
      return;
   }
   free_bsr_item((BSR *)bsr->volume);
   free_bsr_item((BSR *)bsr->client);
   free_bsr_item((BSR *)bsr->sessid);
   free_bsr_item((BSR *)bsr->sesstime);
   free_bsr_item((BSR *)bsr->volfile);
   free_bsr_item((BSR *)bsr->volblock);
   free_bsr_item((BSR *)bsr->JobId);
   free_bsr_item((BSR *)bsr->job);
   free_bsr_item((BSR *)bsr->FileIndex);
   free_bsr_item((BSR *)bsr->JobType);
   free_bsr_item((BSR *)bsr->JobLevel);
   free_bsr(bsr->next);
   free(bsr);
}

/*****************************************************************
 * Routines for handling volumes     
 */
VOL_LIST *new_vol()
{
   VOL_LIST *vol;
   vol = (VOL_LIST *)malloc(sizeof(VOL_LIST));
   memset(vol, 0, sizeof(VOL_LIST));
   return vol;
}

/* 
 * Add current volume to end of list, only if the Volume
 * is not already in the list.
 *
 *   returns: 1 if volume added
 *	      0 if volume already in list
 */
int add_vol(JCR *jcr, VOL_LIST *vol)
{
   VOL_LIST *next = jcr->VolList;

   if (!next) { 		      /* list empty ? */
      jcr->VolList = vol;	      /* yes, add volume */
   } else {
      for ( ; next->next; next=next->next) {
	 if (strcmp(vol->VolumeName, next->VolumeName) == 0) {
	    if (vol->start_file < next->start_file) {
	       next->start_file = vol->start_file;
	    }
	    return 0;		      /* already in list */
	 }
      }
      if (strcmp(vol->VolumeName, next->VolumeName) == 0) {
	 if (vol->start_file < next->start_file) {
	    next->start_file = vol->start_file;
	 }
	 return 0;		      /* already in list */
      }
      next->next = vol; 	      /* add volume */
   }
   return 1;
}

void free_vol_list(JCR *jcr)
{
   VOL_LIST *next = jcr->VolList;
   VOL_LIST *tmp;

   for ( ; next; ) {
      tmp = next->next;
      free(next);
      next = tmp;
   }
   jcr->VolList = NULL;
}

/*
 * Create a list of Volumes (and Slots and Start positions) to be
 *  used in the current restore job.
 */
void create_vol_list(JCR *jcr)
{
   char *p, *n;
   VOL_LIST *vol;

   /* 
    * Build a list of volumes to be processed
    */
   jcr->NumVolumes = 0;
   jcr->CurVolume = 0;
   if (jcr->bsr) {
      BSR *bsr = jcr->bsr;
      if (!bsr->volume || !bsr->volume->VolumeName) {
	 return;
      }
      for ( ; bsr; bsr=bsr->next) {
	 BSR_VOLUME *bsrvol;
	 BSR_VOLFILE *volfile;
	 uint32_t sfile = UINT32_MAX;

	 /* Find minimum start file so that we can forward space to it */
	 for (volfile = bsr->volfile; volfile; volfile=volfile->next) {
	    if (volfile->sfile < sfile) {
	       sfile = volfile->sfile;
	    }
	 }
	 /* Now add volumes for this bsr */
	 for (bsrvol = bsr->volume; bsrvol; bsrvol=bsrvol->next) {
	    vol = new_vol();
	    strcpy(vol->VolumeName, bsrvol->VolumeName);
	    vol->start_file = sfile;
	    if (add_vol(jcr, vol)) {
	       jcr->NumVolumes++;
               Dmsg1(400, "Added volume %s\n", vol->VolumeName);
	    } else {
               Dmsg1(400, "Duplicate volume %s\n", vol->VolumeName);
	       free((char *)vol);
	    }
	    sfile = 0;		      /* start at beginning of second volume */
	 }
      }
   } else {
      /* This is the old way -- deprecated */ 
      for (p = jcr->VolumeName; p && *p; ) {
         n = strchr(p, '|');             /* volume name separator */
	 if (n) {
	    *n++ = 0;			 /* Terminate name */
	 }
	 vol = new_vol();
	 strcpy(vol->VolumeName, p);
	 if (add_vol(jcr, vol)) {
	    jcr->NumVolumes++;
	 } else {
	    free((char *)vol);
	 }
	 p = n;
      }
   }
}
