/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2002-2007 Free Software Foundation Europe e.V.

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
 *
 *  Program to copy a Bacula from one volume to another.
 *
 *   Kern E. Sibbald, October 2002
 *
 *
 *   Version $Id$
 */

#include "bacula.h"
#include "stored.h"

/* Dummy functions */
int generate_daemon_event(JCR *jcr, const char *event) { return 1; }

/* Forward referenced functions */
static bool record_cb(DCR *dcr, DEV_RECORD *rec);


/* Global variables */
static DEVICE *in_dev = NULL;
static DEVICE *out_dev = NULL;
static JCR *in_jcr;                    /* input jcr */
static JCR *out_jcr;                   /* output jcr */
static BSR *bsr = NULL;
static const char *wd = "/tmp";
static int list_records = 0;
static uint32_t records = 0;
static uint32_t jobs = 0;
static DEV_BLOCK *out_block;

#define CONFIG_FILE "bacula-sd.conf"
char *configfile = NULL;
STORES *me = NULL;                    /* our Global resource */
bool forge_on = false;                /* proceed inspite of I/O errors */
pthread_mutex_t device_release_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t wait_device_release = PTHREAD_COND_INITIALIZER;


static void usage()
{
   fprintf(stderr, _(
PROG_COPYRIGHT
"\nVersion: %s (%s)\n\n"
"Usage: bcopy [-d debug_level] <input-archive> <output-archive>\n"
"       -b bootstrap      specify a bootstrap file\n"
"       -c <file>         specify configuration file\n"
"       -d <nn>           set debug level to nn\n"
"       -i                specify input Volume names (separated by |)\n"
"       -o                specify output Volume names (separated by |)\n"
"       -p                proceed inspite of errors\n"
"       -v                verbose\n"
"       -w <dir>          specify working directory (default /tmp)\n"
"       -?                print this message\n\n"), 2002, VERSION, BDATE);
   exit(1);
}

int main (int argc, char *argv[])
{
   int ch;
   char *iVolumeName = NULL;
   char *oVolumeName = NULL;
   bool ignore_label_errors = false;
   bool ok;

   setlocale(LC_ALL, "");
   bindtextdomain("bacula", LOCALEDIR);
   textdomain("bacula");
   init_stack_dump();

   my_name_is(argc, argv, "bcopy");
   init_msg(NULL, NULL);

   while ((ch = getopt(argc, argv, "b:c:d:i:o:pvw:?")) != -1) {
      switch (ch) {
      case 'b':
         bsr = parse_bsr(NULL, optarg);
         break;

      case 'c':                    /* specify config file */
         if (configfile != NULL) {
            free(configfile);
         }
         configfile = bstrdup(optarg);
         break;

      case 'd':                    /* debug level */
         debug_level = atoi(optarg);
         if (debug_level <= 0)
            debug_level = 1;
         break;

      case 'i':                    /* input Volume name */
         iVolumeName = optarg;
         break;

      case 'o':                    /* output Volume name */
         oVolumeName = optarg;
         break;

      case 'p':
         ignore_label_errors = true;
         forge_on = true;
         break;

      case 'v':
         verbose++;
         break;

      case 'w':
         wd = optarg;
         break;

      case '?':
      default:
         usage();

      }
   }
   argc -= optind;
   argv += optind;

   if (argc != 2) {
      Pmsg0(0, _("Wrong number of arguments: \n"));
      usage();
   }

   OSDependentInit();

   working_directory = wd;

   if (configfile == NULL) {
      configfile = bstrdup(CONFIG_FILE);
   }

   parse_config(configfile);

   /* Setup and acquire input device for reading */
   Dmsg0(100, "About to setup input jcr\n");
   in_jcr = setup_jcr("bcopy", argv[0], bsr, iVolumeName, 1); /* read device */
   if (!in_jcr) {
      exit(1);
   }
   in_jcr->ignore_label_errors = ignore_label_errors;
   in_dev = in_jcr->dcr->dev;
   if (!in_dev) {
      exit(1);
   }

   /* Setup output device for writing */
   Dmsg0(100, "About to setup output jcr\n");
   out_jcr = setup_jcr("bcopy", argv[1], bsr, oVolumeName, 0); /* no acquire */
   if (!out_jcr) {
      exit(1);
   }
   out_dev = out_jcr->dcr->dev;
   if (!out_dev) {
      exit(1);
   }
   Dmsg0(100, "About to acquire device for writing\n");
   /* For we must now acquire the device for writing */
   out_dev->r_dlock();
   if (out_dev->open(out_jcr->dcr, OPEN_READ_WRITE) < 0) {
      Emsg1(M_FATAL, 0, _("dev open failed: %s\n"), out_dev->errmsg);
      out_dev->dunlock();
      exit(1);
   }
   out_dev->dunlock();
   if (!acquire_device_for_append(out_jcr->dcr)) {
      free_jcr(in_jcr);
      exit(1);
   }
   out_block = out_jcr->dcr->block;

   ok = read_records(in_jcr->dcr, record_cb, mount_next_read_volume);
   if (ok || out_dev->can_write()) {
      if (!write_block_to_device(out_jcr->dcr)) {
         Pmsg0(000, _("Write of last block failed.\n"));
      }
   }

   Pmsg2(000, _("%u Jobs copied. %u records copied.\n"), jobs, records);

   free_jcr(in_jcr);
   free_jcr(out_jcr);

   in_dev->term();
   out_dev->term();
   return 0;
}


/*
 * read_records() calls back here for each record it gets
 */
static bool record_cb(DCR *in_dcr, DEV_RECORD *rec)
{
   if (list_records) {
      Pmsg5(000, _("Record: SessId=%u SessTim=%u FileIndex=%d Stream=%d len=%u\n"),
            rec->VolSessionId, rec->VolSessionTime, rec->FileIndex,
            rec->Stream, rec->data_len);
   }
   /*
    * Check for Start or End of Session Record
    *
    */
   if (rec->FileIndex < 0) {

      if (verbose > 1) {
         dump_label_record(in_dcr->dev, rec, 1);
      }
      switch (rec->FileIndex) {
      case PRE_LABEL:
         Pmsg0(000, _("Volume is prelabeled. This volume cannot be copied.\n"));
         return false;
      case VOL_LABEL:
         Pmsg0(000, _("Volume label not copied.\n"));
         return true;
      case SOS_LABEL:
         jobs++;
         break;
      case EOS_LABEL:
         while (!write_record_to_block(out_block, rec)) {
            Dmsg2(150, "!write_record_to_block data_len=%d rem=%d\n", rec->data_len,
                       rec->remainder);
            if (!write_block_to_device(out_jcr->dcr)) {
               Dmsg2(90, "Got write_block_to_dev error on device %s: ERR=%s\n",
                  out_dev->print_name(), out_dev->bstrerror());
               Jmsg(out_jcr, M_FATAL, 0, _("Cannot fixup device error. %s\n"),
                     out_dev->bstrerror());
               return false;
            }
         }
         if (!write_block_to_device(out_jcr->dcr)) {
            Dmsg2(90, "Got write_block_to_dev error on device %s: ERR=%s\n",
               out_dev->print_name(), out_dev->bstrerror());
            Jmsg(out_jcr, M_FATAL, 0, _("Cannot fixup device error. %s\n"),
                  out_dev->bstrerror());
            return false;
         }
         return true;
      case EOM_LABEL:
         Pmsg0(000, _("EOM label not copied.\n"));
         return true;
      case EOT_LABEL:              /* end of all tapes */
         Pmsg0(000, _("EOT label not copied.\n"));
         return true;
      default:
         return true;
      }
   }

   /*  Write record */
   records++;
   while (!write_record_to_block(out_block, rec)) {
      Dmsg2(150, "!write_record_to_block data_len=%d rem=%d\n", rec->data_len,
                 rec->remainder);
      if (!write_block_to_device(out_jcr->dcr)) {
         Dmsg2(90, "Got write_block_to_dev error on device %s: ERR=%s\n",
            out_dev->print_name(), out_dev->bstrerror());
         Jmsg(out_jcr, M_FATAL, 0, _("Cannot fixup device error. %s\n"),
               out_dev->bstrerror());
         return false;
      }
   }
   return true;
}


/* Dummies to replace askdir.c */
bool    dir_find_next_appendable_volume(DCR *dcr) { return 1;}
bool    dir_update_volume_info(DCR *dcr, bool relabel) { return 1; }
bool    dir_create_jobmedia_record(DCR *dcr) { return 1; }
bool    dir_ask_sysop_to_create_appendable_volume(DCR *dcr) { return 1; }
bool    dir_update_file_attributes(DCR *dcr, DEV_RECORD *rec) { return 1;}
bool    dir_send_job_status(JCR *jcr) {return 1;}


bool dir_ask_sysop_to_mount_volume(DCR *dcr)
{
   DEVICE *dev = dcr->dev;
   fprintf(stderr, _("Mount Volume \"%s\" on device %s and press return when ready: "),
      dcr->VolumeName, dev->print_name());
   dev->close();
   getchar();
   return true;
}

bool dir_get_volume_info(DCR *dcr, enum get_vol_info_rw  writing)
{
   Dmsg0(100, "Fake dir_get_volume_info\n");
   bstrncpy(dcr->VolCatInfo.VolCatName, dcr->VolumeName, sizeof(dcr->VolCatInfo.VolCatName));
   dcr->VolCatInfo.VolCatParts = find_num_dvd_parts(dcr);
   Dmsg2(500, "Vol=%s num_parts=%d\n", dcr->VolCatInfo.VolCatName, dcr->VolCatInfo.VolCatParts);
   return 1;
}
