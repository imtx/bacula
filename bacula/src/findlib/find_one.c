/*

   This file is based on GNU TAR source code. Except for a few key
   ideas, it has been entirely rewritten for Bacula.

      Kern Sibbald, MM

   Thanks to the TAR programmers.

     Version $Id$

 */
/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2006 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version two of the GNU General Public
   License as published by the Free Software Foundation plus additions
   that are listed in the file LICENSE.

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

#include "bacula.h"
#include "find.h"
#ifdef HAVE_DARWIN_OS
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/attr.h>
#endif

extern int32_t name_max;              /* filename max length */
extern int32_t path_max;              /* path name max length */

/*
 * Structure for keeping track of hard linked files, we
 *   keep an entry for each hardlinked file that we save,
 *   which is the first one found. For all the other files that
 *   are linked to this one, we save only the directory
 *   entry so we can link it.
 */
struct f_link {
    struct f_link *next;
    dev_t dev;                        /* device */
    ino_t ino;                        /* inode with device is unique */
    short linkcount;
    uint32_t FileIndex;               /* Bacula FileIndex of this file */
    char name[1];                     /* The name */
};

typedef struct f_link link_t;
#define LINK_HASHTABLE_BITS 16
#define LINK_HASHTABLE_SIZE (1<<LINK_HASHTABLE_BITS)
#define LINK_HASHTABLE_MASK (LINK_HASHTABLE_SIZE-1)

static inline int LINKHASH(const struct stat &info)
{
    int hash = info.st_dev;
    unsigned long long i = info.st_ino;
    hash ^= i;
    i >>= 16;
    hash ^= i;
    i >>= 16;
    hash ^= i;
    i >>= 16;
    hash ^= i;
    return hash & LINK_HASHTABLE_MASK;
}

static void free_dir_ff_pkt(FF_PKT *dir_ff_pkt)
{
   free(dir_ff_pkt->fname);
   free(dir_ff_pkt->link);
   free_pool_memory(dir_ff_pkt->sys_fname);
   free(dir_ff_pkt);
}

/*
 * Check to see if we allow the file system type of a file or directory.
 * If we do not have a list of file system types, we accept anything.
 */
static int accept_fstype(FF_PKT *ff, void *dummy) {
   int i;
   char fs[1000];
   bool accept = true;

   if (ff->fstypes.size()) {
      accept = false;
      if (!fstype(ff->fname, fs, sizeof(fs))) {
         Dmsg1(50, "Cannot determine file system type for \"%s\"\n", ff->fname);
      } else {
         for (i = 0; i < ff->fstypes.size(); ++i) {
            if (strcmp(fs, (char *)ff->fstypes.get(i)) == 0) {
               Dmsg2(100, "Accepting fstype %s for \"%s\"\n", fs, ff->fname);
               accept = true;
               break;
            }
            Dmsg3(200, "fstype %s for \"%s\" does not match %s\n", fs,
                  ff->fname, ff->fstypes.get(i));
         }
      }
   }
   return accept;
}

/*
 * Check to see if we allow the drive type of a file or directory.
 * If we do not have a list of drive types, we accept anything.
 */
static int accept_drivetype(FF_PKT *ff, void *dummy) {
   int i;
   char dt[100];
   bool accept = true;

   if (ff->drivetypes.size()) {
      accept = false;
      if (!drivetype(ff->fname, dt, sizeof(dt))) {
         Dmsg1(50, "Cannot determine drive type for \"%s\"\n", ff->fname);
      } else {
         for (i = 0; i < ff->drivetypes.size(); ++i) {
            if (strcmp(dt, (char *)ff->drivetypes.get(i)) == 0) {
               Dmsg2(100, "Accepting drive type %s for \"%s\"\n", dt, ff->fname);
               accept = true;
               break;
            }
            Dmsg3(200, "drive type %s for \"%s\" does not match %s\n", dt,
                  ff->fname, ff->drivetypes.get(i));
         }
      }
   }
   return accept;
}

/*
 * This function determines whether we can use getattrlist()
 * It's odd, but we have to use the function to determine that...
 * Also, the man pages talk about things as if they were implemented.
 *
 * On Mac OS X, this succesfully differentiates between HFS+ and UFS
 * volumes, which makes me trust it is OK for others, too.
 */
static bool volume_has_attrlist(const char *fname)
{
#ifdef HAVE_DARWIN_OS
   struct statfs st;
   struct volinfo_struct {
      unsigned long length;               /* Mandatory field */
      vol_capabilities_attr_t info;       /* Volume capabilities */
   } vol;
   struct attrlist attrList;

   memset(&attrList, 0, sizeof(attrList));
   attrList.bitmapcount = ATTR_BIT_MAP_COUNT;
   attrList.volattr = ATTR_VOL_INFO | ATTR_VOL_CAPABILITIES;
   if (statfs(fname, &st) == 0) {
      /* We need to check on the mount point */
      if (getattrlist(st.f_mntonname, &attrList, &vol, sizeof(vol), FSOPT_NOFOLLOW) == 0
            && (vol.info.capabilities[VOL_CAPABILITIES_INTERFACES] & VOL_CAP_INT_ATTRLIST)
            && (vol.info.valid[VOL_CAPABILITIES_INTERFACES] & VOL_CAP_INT_ATTRLIST)) {
         return true;
      }
   }
#endif
   return false;
}

/* check if a file have changed during backup and display an error */
bool has_file_changed(JCR *jcr, FF_PKT *ff_pkt)
{
   struct stat statp;
   Dmsg1(500, "has_file_changed fname=%s\n",ff_pkt->fname);

   if (ff_pkt->type != FT_REG) { /* not a regular file */
      return false;
   }

   if (lstat(ff_pkt->fname, &statp) != 0) {
      berrno be;
      Jmsg(jcr, M_WARNING, 0, 
	   _("Cannot stat file %s: ERR=%s\n"),ff_pkt->fname,be.strerror());
      return true;
   }

   if (statp.st_mtime != ff_pkt->statp.st_mtime) {
      /* TODO: add time of changes */
      Jmsg(jcr, M_ERROR, 0, _("%s mtime changed during backup.\n"), ff_pkt->fname);
      return true;
   }

   if (statp.st_ctime != ff_pkt->statp.st_ctime) {
      /* TODO: add time of changes */
      Jmsg(jcr, M_ERROR, 0, _("%s ctime changed during backup.\n"), ff_pkt->fname);
      return true;
   }
  
   if (statp.st_size != ff_pkt->statp.st_size) {
      /* TODO: add size change */
      Jmsg(jcr, M_ERROR, 0, _("%s size changed during backup.\n"),ff_pkt->fname);
      return true;
   }

   if ((statp.st_blksize != ff_pkt->statp.st_blksize) ||
       (statp.st_blocks  != ff_pkt->statp.st_blocks)) {
      /* TODO: add size change */
      Jmsg(jcr, M_ERROR, 0, _("%s size changed during backup.\n"),ff_pkt->fname);
      return true;
   }

   return false;
}

/*
 * Find a single file.
 * handle_file is the callback for handling the file.
 * p is the filename
 * parent_device is the device we are currently on
 * top_level is 1 when not recursing or 0 when
 *  descending into a directory.
 */
int
find_one_file(JCR *jcr, FF_PKT *ff_pkt, 
               int handle_file(FF_PKT *ff, void *hpkt, bool top_level),
               void *pkt, char *fname, dev_t parent_device, bool top_level)
{
   struct utimbuf restore_times;
   int rtn_stat;
   int len;

   ff_pkt->fname = ff_pkt->link = fname;

   if (lstat(fname, &ff_pkt->statp) != 0) {
       /* Cannot stat file */
       ff_pkt->type = FT_NOSTAT;
       ff_pkt->ff_errno = errno;
       return handle_file(ff_pkt, pkt, top_level);
   }

   Dmsg1(300, "File ----: %s\n", fname);

   /* Save current times of this directory in case we need to
    * reset them because the user doesn't want them changed.
    */
   restore_times.actime = ff_pkt->statp.st_atime;
   restore_times.modtime = ff_pkt->statp.st_mtime;

   /*
    * We check for allowed fstypes and drivetypes at top_level and fstype change (below).
    */
   if (top_level) {
      if (!accept_fstype(ff_pkt, NULL)) {
         ff_pkt->type = FT_INVALIDFS;
         if (ff_pkt->flags & FO_KEEPATIME) {
            utime(fname, &restore_times);
         }

         char fs[100];

         if (!fstype(ff_pkt->fname, fs, sizeof(fs))) {
             bstrncpy(fs, "unknown", sizeof(fs));
         }

         Jmsg(jcr, M_INFO, 0, _("Top level directory \"%s\" has unlisted fstype \"%s\"\n"), fname, fs);
         return 1;      /* Just ignore this error - or the whole backup is cancelled */
      }
      if (!accept_drivetype(ff_pkt, NULL)) {
         ff_pkt->type = FT_INVALIDDT;
         if (ff_pkt->flags & FO_KEEPATIME) {
            utime(fname, &restore_times);
         }

         char dt[100];

         if (!drivetype(ff_pkt->fname, dt, sizeof(dt))) {
             bstrncpy(dt, "unknown", sizeof(dt));
         }

         Jmsg(jcr, M_INFO, 0, _("Top level directory \"%s\" has an unlisted drive type \"%s\"\n"), fname, dt);
         return 1;      /* Just ignore this error - or the whole backup is cancelled */
      }
      ff_pkt->volhas_attrlist = volume_has_attrlist(fname);
   }

   /*
    * If this is an Incremental backup, see if file was modified
    * since our last "save_time", presumably the last Full save
    * or Incremental.
    */
   if (ff_pkt->incremental && !S_ISDIR(ff_pkt->statp.st_mode)) {
      Dmsg1(300, "Non-directory incremental: %s\n", ff_pkt->fname);
      /* Not a directory */
      if (ff_pkt->statp.st_mtime < ff_pkt->save_time
          && ((ff_pkt->flags & FO_MTIMEONLY) ||
              ff_pkt->statp.st_ctime < ff_pkt->save_time)) {
         /* Incremental option, file not changed */
         ff_pkt->type = FT_NOCHG;
         return handle_file(ff_pkt, pkt, top_level);
      }
   }

#ifdef HAVE_DARWIN_OS
   if (ff_pkt->flags & FO_HFSPLUS && ff_pkt->volhas_attrlist
         && S_ISREG(ff_pkt->statp.st_mode)) {
       /* TODO: initialise attrList once elsewhere? */
       struct attrlist attrList;
       memset(&attrList, 0, sizeof(attrList));
       attrList.bitmapcount = ATTR_BIT_MAP_COUNT;
       attrList.commonattr = ATTR_CMN_FNDRINFO;
       attrList.fileattr = ATTR_FILE_RSRCLENGTH;
       if (getattrlist(fname, &attrList, &ff_pkt->hfsinfo,
                sizeof(ff_pkt->hfsinfo), FSOPT_NOFOLLOW) != 0) {
          ff_pkt->type = FT_NOSTAT;
          ff_pkt->ff_errno = errno;
          return handle_file(ff_pkt, pkt, top_level);
       }
   }
#endif

/* ***FIXME*** implement this */
#if xxxxxxx
   /* See if we are trying to dump the archive.  */
   if (ar_dev && ff_pkt->statp.st_dev == ar_dev && ff_pkt->statp.st_ino == ar_ino) {
       ff_pkt->type = FT_ISARCH;
       return handle_file(ff_pkt, pkt, top_level);
   }
#endif
   ff_pkt->LinkFI = 0;
   /*
    * Handle hard linked files
    *
    * Maintain a list of hard linked files already backed up. This
    *  allows us to ensure that the data of each file gets backed
    *  up only once.
    */
   if (!(ff_pkt->flags & FO_NO_HARDLINK)
       && ff_pkt->statp.st_nlink > 1
       && (S_ISREG(ff_pkt->statp.st_mode)
           || S_ISCHR(ff_pkt->statp.st_mode)
           || S_ISBLK(ff_pkt->statp.st_mode)
           || S_ISFIFO(ff_pkt->statp.st_mode)
           || S_ISSOCK(ff_pkt->statp.st_mode))) {

       struct f_link *lp;
       if (ff_pkt->linkhash == NULL) {
           ff_pkt->linkhash = (link_t **)bmalloc(LINK_HASHTABLE_SIZE * sizeof(link_t *));
           memset(ff_pkt->linkhash, 0, LINK_HASHTABLE_SIZE * sizeof(link_t *));
       }
       const int linkhash = LINKHASH(ff_pkt->statp);

      /* Search link list of hard linked files */
       for (lp = ff_pkt->linkhash[linkhash]; lp; lp = lp->next)
         if (lp->ino == (ino_t)ff_pkt->statp.st_ino &&
             lp->dev == (dev_t)ff_pkt->statp.st_dev) {
             /* If we have already backed up the hard linked file don't do it again */
             if (strcmp(lp->name, fname) == 0) {
                return 1;             /* ignore */
             }
             ff_pkt->link = lp->name;
             ff_pkt->type = FT_LNKSAVED;       /* Handle link, file already saved */
             ff_pkt->LinkFI = lp->FileIndex;
             return handle_file(ff_pkt, pkt, top_level);
         }

      /* File not previously dumped. Chain it into our list. */
      len = strlen(fname) + 1;
      lp = (struct f_link *)bmalloc(sizeof(struct f_link) + len);
      lp->ino = ff_pkt->statp.st_ino;
      lp->dev = ff_pkt->statp.st_dev;
      bstrncpy(lp->name, fname, len);
       lp->next = ff_pkt->linkhash[linkhash];
       ff_pkt->linkhash[linkhash] = lp;
      ff_pkt->linked = lp;            /* mark saved link */
   } else {
      ff_pkt->linked = NULL;
   }

   /* This is not a link to a previously dumped file, so dump it.  */
   if (S_ISREG(ff_pkt->statp.st_mode)) {
      boffset_t sizeleft;

      sizeleft = ff_pkt->statp.st_size;

      /* Don't bother opening empty, world readable files.  Also do not open
         files when archive is meant for /dev/null.  */
      if (ff_pkt->null_output_device || (sizeleft == 0
              && MODE_RALL == (MODE_RALL & ff_pkt->statp.st_mode))) {
         ff_pkt->type = FT_REGE;
      } else {
         ff_pkt->type = FT_REG;
      }
      rtn_stat = handle_file(ff_pkt, pkt, top_level);
      if (ff_pkt->linked) {
         ff_pkt->linked->FileIndex = ff_pkt->FileIndex;
      }
      if (ff_pkt->flags & FO_KEEPATIME) {
         utime(fname, &restore_times);
      }       
      return rtn_stat;


   } else if (S_ISLNK(ff_pkt->statp.st_mode)) {  /* soft link */
      int size;
      char *buffer = (char *)alloca(path_max + name_max + 102);

      size = readlink(fname, buffer, path_max + name_max + 101);
      if (size < 0) {
         /* Could not follow link */
         ff_pkt->type = FT_NOFOLLOW;
         ff_pkt->ff_errno = errno;
         rtn_stat = handle_file(ff_pkt, pkt, top_level);
         if (ff_pkt->linked) {
            ff_pkt->linked->FileIndex = ff_pkt->FileIndex;
         }
         return rtn_stat;
      }
      buffer[size] = 0;
      ff_pkt->link = buffer;          /* point to link */
      ff_pkt->type = FT_LNK;          /* got a real link */
      rtn_stat = handle_file(ff_pkt, pkt, top_level);
      if (ff_pkt->linked) {
         ff_pkt->linked->FileIndex = ff_pkt->FileIndex;
      }
      return rtn_stat;

   } else if (S_ISDIR(ff_pkt->statp.st_mode)) {
      DIR *directory;
      struct dirent *entry, *result;
      char *link;
      int link_len;
      int len;
      int status;
      dev_t our_device = ff_pkt->statp.st_dev;
      bool recurse = true;
      bool volhas_attrlist = ff_pkt->volhas_attrlist;    /* Remember this if we recurse */

      /*
       * If we are using Win32 (non-portable) backup API, don't check
       *  access as everything is more complicated, and
       *  in principle, we should be able to access everything.
       */
      if (!have_win32_api() || (ff_pkt->flags & FO_PORTABLE)) {
         if (access(fname, R_OK) == -1 && geteuid() != 0) {
            /* Could not access() directory */
            ff_pkt->type = FT_NOACCESS;
            ff_pkt->ff_errno = errno;
            rtn_stat = handle_file(ff_pkt, pkt, top_level);
            if (ff_pkt->linked) {
               ff_pkt->linked->FileIndex = ff_pkt->FileIndex;
            }
            return rtn_stat;
         }
      }

      /* Build a canonical directory name with a trailing slash in link var */
      len = strlen(fname);
      link_len = len + 200;
      link = (char *)bmalloc(link_len + 2);
      bstrncpy(link, fname, link_len);
      /* Strip all trailing slashes */
      while (len >= 1 && IsPathSeparator(link[len - 1]))
        len--;
      link[len++] = '/';             /* add back one */
      link[len] = 0;

      ff_pkt->link = link;
      if (ff_pkt->incremental &&
          (ff_pkt->statp.st_mtime < ff_pkt->save_time &&
             ((ff_pkt->flags & FO_MTIMEONLY) ||
               ff_pkt->statp.st_ctime < ff_pkt->save_time))) {
         /* Incremental option, directory entry not changed */
         ff_pkt->type = FT_DIRNOCHG;
      } else {
         ff_pkt->type = FT_DIRBEGIN;
      }
      /*
       * Note, we return the directory to the calling program (handle_file)
       * when we first see the directory (FT_DIRBEGIN.
       * This allows the program to apply matches and make a
       * choice whether or not to accept it.  If it is accepted, we
       * do not immediately save it, but do so only after everything
       * in the directory is seen (i.e. the FT_DIREND).
       */
      rtn_stat = handle_file(ff_pkt, pkt, top_level);
      if (rtn_stat < 1) {             /* ignore or error status */
         free(link);
         return rtn_stat;
      }
      /* Done with DIRBEGIN, next call will be DIREND */
      if (ff_pkt->type == FT_DIRBEGIN) {
         ff_pkt->type = FT_DIREND;
      }

      /*
       * Create a temporary ff packet for this directory
       *   entry, and defer handling the directory until
       *   we have recursed into it.  This saves the
       *   directory after all files have been processed, and
       *   during the restore, the directory permissions will
       *   be reset after all the files have been restored.
       */
      Dmsg1(300, "Create temp ff packet for dir: %s\n", ff_pkt->fname);
      FF_PKT *dir_ff_pkt = (FF_PKT *)bmalloc(sizeof(FF_PKT));
      memcpy(dir_ff_pkt, ff_pkt, sizeof(FF_PKT));
      dir_ff_pkt->fname = bstrdup(ff_pkt->fname);
      dir_ff_pkt->link = bstrdup(ff_pkt->link);
      dir_ff_pkt->sys_fname = get_pool_memory(PM_FNAME);
      dir_ff_pkt->included_files_list = NULL;
      dir_ff_pkt->excluded_files_list = NULL;
      dir_ff_pkt->excluded_paths_list = NULL;
      dir_ff_pkt->linkhash = NULL;

      /*
       * Do not descend into subdirectories (recurse) if the
       * user has turned it off for this directory.
       *
       * If we are crossing file systems, we are either not allowed
       * to cross, or we may be restricted by a list of permitted
       * file systems.
       */
      if (!top_level && ff_pkt->flags & FO_NO_RECURSION) {
         ff_pkt->type = FT_NORECURSE;
         recurse = false;
      } else if (!top_level && parent_device != ff_pkt->statp.st_dev) {
         if(!(ff_pkt->flags & FO_MULTIFS)) {
            ff_pkt->type = FT_NOFSCHG;
            recurse = false;
         } else if (!accept_fstype(ff_pkt, NULL)) {
            ff_pkt->type = FT_INVALIDFS;
            recurse = false;
         } else {
            ff_pkt->volhas_attrlist = volume_has_attrlist(fname);
         }
      }
      /* If not recursing, just backup dir and return */
      if (!recurse) {
         rtn_stat = handle_file(ff_pkt, pkt, top_level);
         if (ff_pkt->linked) {
            ff_pkt->linked->FileIndex = ff_pkt->FileIndex;
         }
         free(link);
         free_dir_ff_pkt(dir_ff_pkt);
         ff_pkt->link = ff_pkt->fname;     /* reset "link" */
         if (ff_pkt->flags & FO_KEEPATIME) {
            utime(fname, &restore_times);
         }
         return rtn_stat;
      }

      ff_pkt->link = ff_pkt->fname;     /* reset "link" */

      /*
       * Descend into or "recurse" into the directory to read
       *   all the files in it.
       */
      errno = 0;
      if ((directory = opendir(fname)) == NULL) {
         ff_pkt->type = FT_NOOPEN;
         ff_pkt->ff_errno = errno;
         rtn_stat = handle_file(ff_pkt, pkt, top_level);
         if (ff_pkt->linked) {
            ff_pkt->linked->FileIndex = ff_pkt->FileIndex;
         }
         free(link);
         free_dir_ff_pkt(dir_ff_pkt);
         return rtn_stat;
      }

      /*
       * Process all files in this directory entry (recursing).
       *    This would possibly run faster if we chdir to the directory
       *    before traversing it.
       */
      rtn_stat = 1;
      entry = (struct dirent *)malloc(sizeof(struct dirent) + name_max + 100);
      for ( ; !job_canceled(jcr); ) {
         char *p, *q;
         int i;

         status  = readdir_r(directory, entry, &result);
         if (status != 0 || result == NULL) {
//          Dmsg2(99, "readdir returned stat=%d result=0x%x\n",
//             status, (long)result);
            break;
         }
         ASSERT(name_max+1 > (int)sizeof(struct dirent) + (int)NAMELEN(entry));
         p = entry->d_name;
         /* Skip `.', `..', and excluded file names.  */
         if (p[0] == '\0' || (p[0] == '.' && (p[1] == '\0' ||
             (p[1] == '.' && p[2] == '\0')))) {
            continue;
         }

         if ((int)NAMELEN(entry) + len >= link_len) {
             link_len = len + NAMELEN(entry) + 1;
             link = (char *)brealloc(link, link_len + 1);
         }
         q = link + len;
         for (i=0; i < (int)NAMELEN(entry); i++) {
            *q++ = *p++;
         }
         *q = 0;
         if (!file_is_excluded(ff_pkt, link)) {
            rtn_stat = find_one_file(jcr, ff_pkt, handle_file, pkt, link, our_device, false);
            if (ff_pkt->linked) {
               ff_pkt->linked->FileIndex = ff_pkt->FileIndex;
            }
         }
      }
      closedir(directory);
      free(link);
      free(entry);

      /*
       * Now that we have recursed through all the files in the
       *  directory, we "save" the directory so that after all
       *  the files are restored, this entry will serve to reset
       *  the directory modes and dates.  Temp directory values
       *  were used without this record.
       */
      handle_file(dir_ff_pkt, pkt, top_level);       /* handle directory entry */
      if (ff_pkt->linked) {
         ff_pkt->linked->FileIndex = dir_ff_pkt->FileIndex;
      }
      free_dir_ff_pkt(dir_ff_pkt);

      if (ff_pkt->flags & FO_KEEPATIME) {
         utime(fname, &restore_times);
      }
      ff_pkt->volhas_attrlist = volhas_attrlist;      /* Restore value in case it changed. */
      return rtn_stat;
   } /* end check for directory */

   /*
    * If it is explicitly mentioned (i.e. top_level) and is
    *  a block device, we do a raw backup of it or if it is
    *  a fifo, we simply read it.
    */
#ifdef HAVE_FREEBSD_OS
   /*
    * On FreeBSD, all block devices are character devices, so
    *   to be able to read a raw disk, we need the check for
    *   a character device.
��� * crw-r-----� 1 root� operator� - 116, 0x00040002 Jun� 9 19:32 /dev/ad0s3
��� * crw-r-----� 1 root� operator� - 116, 0x00040002 Jun� 9 19:32 /dev/rad0s3
    */
   if (top_level && (S_ISBLK(ff_pkt->statp.st_mode) || S_ISCHR(ff_pkt->statp.st_mode))) {
#else
   if (top_level && S_ISBLK(ff_pkt->statp.st_mode)) {
#endif
      ff_pkt->type = FT_RAW;          /* raw partition */
   } else if (top_level && S_ISFIFO(ff_pkt->statp.st_mode) &&
              ff_pkt->flags & FO_READFIFO) {
      ff_pkt->type = FT_FIFO;
   } else {
      /* The only remaining types are special (character, ...) files */
      ff_pkt->type = FT_SPEC;
   }
   rtn_stat = handle_file(ff_pkt, pkt, top_level);
   if (ff_pkt->linked) {
      ff_pkt->linked->FileIndex = ff_pkt->FileIndex;
   }
   return rtn_stat;
}

int term_find_one(FF_PKT *ff)
{
   struct f_link *lp, *lc;
   int count = 0;
   int i;

   
   if (ff->linkhash == NULL) return 0;

   for (i =0 ; i < LINK_HASHTABLE_SIZE; i ++) {
   /* Free up list of hard linked files */
       lp = ff->linkhash[i];
       while (lp) {
      lc = lp;
      lp = lp->next;
      if (lc) {
         free(lc);
         count++;
      }
   }
       ff->linkhash[i] = NULL;
   }
   free(ff->linkhash);
   ff->linkhash = NULL;
   return count;
}
