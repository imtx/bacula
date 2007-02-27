/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2007-2007 Free Software Foundation Europe e.V.

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
 
/*
 *   Version $Id$
 *
 *  Restore Class 
 *
 *   Kern Sibbald, February MMVI
 *
 */ 

#include "bat.h"
#include "restore.h"


prerestoreDialog::prerestoreDialog(Console *console)
{
   m_console = console;               /* keep compiler quiet */
   setupUi(this);
   jobCombo->addItems(console->job_list);
   filesetCombo->addItems(console->fileset_list);
   clientCombo->addItems(console->client_list);
   poolCombo->addItems(console->pool_list);
   storageCombo->addItems(console->storage_list);
   job_name_change(0);
   connect(jobCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(job_name_change(int)));

   this->show();
}

void prerestoreDialog::accept()
{
   QString cmd;

   this->hide();
   
   cmd = QString(
         "restore select current fileset=\"%1\" client=\"%2\" pool=\"%3\" "
             "storage=\"%4\"\n")
             .arg(filesetCombo->currentText())
             .arg(clientCombo->currentText())
             .arg(poolCombo->currentText())
             .arg(storageCombo->currentText());

   m_console->write(cmd);
   m_console->display_text(cmd);
   new restoreDialog(m_console);
   delete this;
}


void prerestoreDialog::reject()
{
   mainWin->set_status("Canceled");
   this->hide();
   delete this;
}


void prerestoreDialog::job_name_change(int index)
{
   job_defaults job_defs;

   (void)index;
   job_defs.job_name = jobCombo->currentText();
   if (m_console->get_job_defaults(job_defs)) {
      filesetCombo->setCurrentIndex(filesetCombo->findText(job_defs.fileset_name, Qt::MatchExactly));
      clientCombo->setCurrentIndex(clientCombo->findText(job_defs.client_name, Qt::MatchExactly));
      poolCombo->setCurrentIndex(poolCombo->findText(job_defs.pool_name, Qt::MatchExactly));
      storageCombo->setCurrentIndex(storageCombo->findText(job_defs.store_name, Qt::MatchExactly));
   }
}

restoreDialog::restoreDialog(Console *console)
{
   m_console = console;
   setupUi(this);
   fillDirectory("/");
   this->show();
}

/*
 * Fill the CList box with files at path
 */
void restoreDialog::fillDirectory(const char *path)
{
   char pathbuf[MAXSTRING];
   char modes[20], user[20], group[20], size[20], date[30];
   char file[1000];
   char marked[10];
   int row = 0;

   m_console->setEnabled(false);
   m_fname = path;


   m_console->displayToPrompt();
   bsnprintf(pathbuf, sizeof(pathbuf), "cd %s", path);
   Dmsg1(100, "%s\n", pathbuf);

   m_console->write(pathbuf);
   m_console->display_text(pathbuf);
   m_console->displayToPrompt();

   m_console-> write_dir("dir");
   m_console->display_text("dir");
   while (m_console->read() > 0) {
      char *p = m_console->msg();
      char *l;
      strip_trailing_junk(p);
      if (*p == '$') {
         break;
      }
      Dmsg1(200, "Got: %s\n", p);
      if (!*p) {
         continue;
      }
      l = p;
      skip_nonspaces(&p);             /* permissions */
      *p++ = 0;
      bstrncpy(modes, l, sizeof(modes));
      skip_spaces(&p);
      skip_nonspaces(&p);             /* link count */
      *p++ = 0;
      skip_spaces(&p);
      l = p;
      skip_nonspaces(&p);             /* user */
      *p++ = 0;
      skip_spaces(&p);
      bstrncpy(user, l, sizeof(user));
      l = p;
      skip_nonspaces(&p);             /* group */
      *p++ = 0;
      bstrncpy(group, l, sizeof(group));
      skip_spaces(&p);
      l = p;
      skip_nonspaces(&p);             /* size */
      *p++ = 0;
      bstrncpy(size, l, sizeof(size));
      skip_spaces(&p);
      l = p;
      skip_nonspaces(&p);             /* date/time */
      skip_spaces(&p);
      skip_nonspaces(&p);
      *p++ = 0;
      bstrncpy(date, l, sizeof(date));
      skip_spaces(&p);
      if (*p == '*') {
         bstrncpy(marked, "x", sizeof(marked));
         p++;
      } else {
         bstrncpy(marked, " ", sizeof(marked));
      }
//    split_path_and_filename(p, &restore->path, &restore->pnl,
//                            &restore->file, &restore->fnl);

//    Dmsg1(000, "restore->fname=%s\n", restore->fname);
//    bstrncpy(file, restore->file, sizeof(file));
      printf("modes=%s user=%s group=%s size=%s date=%s file=%s\n",
         modes, user, group, size, date, file);

//    append to list

      row++;
   }

     m_console->setEnabled(true);
}
