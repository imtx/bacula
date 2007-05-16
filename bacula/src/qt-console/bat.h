#ifndef _BAT_H_
#define _BAT_H_

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
 *   Kern Sibbald, January 2007
 */

#include "mainwin.h"
#include "config.h"
#include "bacula.h"
#include "bat_conf.h"
#include "jcr.h"
#include "console.h"
#include "qstd.h"

using namespace qstd;

extern MainWin *mainWin;
extern QApplication *app;
extern bool g_commDebug;
extern bool g_displayAll;

int bvsnprintf(char *str, int32_t size, const char *format, va_list ap);

#endif /* _BAT_H_ */
