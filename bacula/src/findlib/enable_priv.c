/*
 *  Enable backup privileges for Win32 systems.
 *
 *    Kern Sibbald, May MMIII
 *
 *   Version $Id$
 *
 */
/*
   Copyright (C) 2003-2004 Kern Sibbald and John Walker

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
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA.

 */

#include "bacula.h"
#include "find.h"
#include "jcr.h"


/*=============================================================*/
/*							       */
/*		   * * *  U n i x * * * *		       */
/*							       */
/*=============================================================*/

#if !defined(HAVE_CYGWIN) && !defined(HAVE_WIN32)
    
int enable_backup_privileges(JCR *jcr, int ignore_errors)
 { return 0; }


#endif



/*=============================================================*/
/*							       */
/*		   * * *  W i n 3 2 * * * *		       */
/*							       */
/*=============================================================*/

#if defined(HAVE_CYGWIN) || defined(HAVE_WIN32)

void win_error(JCR *jcr, char *prefix, DWORD lerror);

static int
enable_priv(JCR *jcr, HANDLE hToken, char *name, int ignore_errors)
{
    TOKEN_PRIVILEGES tkp;
    DWORD lerror;

    if (!(p_LookupPrivilegeValue && p_AdjustTokenPrivileges)) {
       return 0;		      /* not avail on this OS */
    }

    // Get the LUID for the security privilege. 
    if (!p_LookupPrivilegeValue(NULL, name,  &tkp.Privileges[0].Luid)) {
       if (!ignore_errors) {  
          win_error(jcr, "LookupPrivilegeValue", GetLastError());
       }
    }

    /* Set the security privilege for this process. */
    tkp.PrivilegeCount = 1;
    tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    p_AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, NULL, NULL);
    lerror = GetLastError();
    if (lerror != ERROR_SUCCESS) {
       if (!ignore_errors) {
	  char buf[200];
          strcpy(buf, "AdjustTokenPrivileges set ");
	  bstrncat(buf, name, sizeof(buf));
	  win_error(jcr, buf, lerror);
       }
       return 0;
    } 
    return 1;
}

/*
 * Setup privileges we think we will need.  We probably do not need
 *  the SE_SECURITY_NAME, but since nothing seems to be working,
 *  we get it hoping to fix the problems.
 */
int enable_backup_privileges(JCR *jcr, int ignore_errors)
{
    HANDLE hToken, hProcess;
    int stat = 0;

    if (!p_OpenProcessToken) {
       return 0;		      /* No avail on this OS */
    }

    hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, GetCurrentProcessId());

    // Get a token for this process. 
    if (!p_OpenProcessToken(hProcess,
	    TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
       if (!ignore_errors) {  
          win_error(jcr, "OpenProcessToken", GetLastError());
       }
       /* Forge on anyway */
    } 

    /* Return a bit map of permissions set. */
    if (enable_priv(jcr, hToken, SE_SECURITY_NAME, ignore_errors)) {
       stat |= 1<<0;
    }
    if (enable_priv(jcr, hToken, SE_BACKUP_NAME, ignore_errors)) {
       stat |= 1<<1;
    }
    if (enable_priv(jcr, hToken, SE_RESTORE_NAME, ignore_errors)) {
       stat |= 1<<2;
    }
    if (enable_priv(jcr, hToken, SE_TAKE_OWNERSHIP_NAME, ignore_errors)) {
       stat |= 1<<3;
    }
    if (enable_priv(jcr, hToken, SE_ASSIGNPRIMARYTOKEN_NAME, ignore_errors)) {
       stat |= 1<<4;
    }
    if (enable_priv(jcr, hToken, SE_SYSTEM_ENVIRONMENT_NAME, ignore_errors)) {
       stat |= 1<<5;
    }
    if (enable_priv(jcr, hToken, SE_CREATE_TOKEN_NAME, ignore_errors)) {
       stat |= 1<<6;
    }
    if (enable_priv(jcr, hToken, SE_MACHINE_ACCOUNT_NAME, ignore_errors)) {
       stat |= 1<<7;
    }
    if (enable_priv(jcr, hToken, SE_TCB_NAME, ignore_errors)) {
       stat |= 1<<8;
    }
    if (stat) {
       stat |= 1<<9;
    }

    CloseHandle(hToken);
    CloseHandle(hProcess);
    return stat;
}

#endif	/* HAVE_CYGWIN */
