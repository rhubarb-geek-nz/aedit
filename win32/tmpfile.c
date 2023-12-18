/**************************************************************************
 *
 *  Copyright 1989-2012, Roger Brown
 *
 *  This file is part of Roger Brown's Aedit.
 *
 *  This program is free software: you can redistribute it and/or modify it
 *  under the terms of the GNU Lesser General Public License as published by the
 *  Free Software Foundation, either version 3 of the License, or (at your
 *  option) any later version.
 * 
 *  This program is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 *  more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>
 *
 */

/* 
 * $Id: tmpfile.c 21 2022-06-30 20:53:56Z rhubarb-geek-nz $
 */

#include <windows.h>
#include <errno.h>
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <sys/stat.h>

#define PATH_MAX		260

FILE *aedit_tmpfile(void)
{
	char dir[PATH_MAX];
	DWORD retval=GetTempPathA (sizeof(dir),dir);
	int fd=-1;
	FILE *file = NULL;
	int x=1;

	if ((retval != 0) && (retval < (sizeof(dir)-1)))
	{
		do
		{
			char xtemplate[sizeof(dir)+15];

			_snprintf(xtemplate,sizeof(xtemplate),"%s\\~%07d.tmp",dir,x++);

			fd=_open (xtemplate,
				_O_BINARY | _O_CREAT | _O_TEMPORARY | _O_EXCL | _O_RDWR,
				_S_IREAD | _S_IWRITE); 
		}
		while ((fd < 0) && (errno == EEXIST));

		if (fd>=0)
		{
			file=_fdopen(fd,"w+b");

			if (!file) 
			{
				int save_errno=errno;
				_close (fd);
				errno=save_errno;
			}
		}
	}

	return file;
}
