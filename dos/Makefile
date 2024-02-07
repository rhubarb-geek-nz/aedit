#
#  Copyright 2021, Roger Brown
#
#  This file is part of RHB aedit.
#
#  This program is free software: you can redistribute it and/or modify it
#  under the terms of the GNU General Public License as published by the
#  Free Software Foundation, either version 3 of the License, or (at your
#  option) any later version.
# 
#  This program is distributed in the hope that it will be useful, but WITHOUT
#  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
#  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
#  more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>
#
#  $Id: Makefile 90 2024-02-07 20:49:10Z rhubarb-geek-nz $

CFLAGS=-I. -dHAVE_TMPFILE -dHAVE_TERMIOS_H= -dHAVE_PWD_H= -dHAVE_CFMAKERAW= -d0
SRCFILES=..\src\aedit.c dosvt100.c
HDRFILES=termios.h pwd.h

all: aedit.exe 286\aedit.exe 386\aedit.exe

clean: .SYMBOLIC
	if exist aedit.exe del aedit.exe
	if exist 286\aedit.exe del 286\aedit.exe
	if exist 386\aedit.exe del 386\aedit.exe
	if exist 286 rmdir 286
	if exist 386 rmdir 386

aedit.exe: $(SRCFILES) $(HDRFILES)
	wcl.exe -0 -bcl=dos -fpc $(SRCFILES) -fe=$@ $(CFLAGS)
	del *.obj

286\aedit.exe: $(SRCFILES) $(HDRFILES) 286
	wcl.exe -2 -bcl=os2 -lp $(SRCFILES) -fe=$@ $(CFLAGS) -I$(%WATCOM)\h\os21x
	del *.obj

386\aedit.exe: $(SRCFILES) $(HDRFILES) 386
	wcl386.exe $(SRCFILES) -fe=$@ $(CFLAGS)
	del *.obj

286 386: .SYMBOLIC
	mkdir $@
