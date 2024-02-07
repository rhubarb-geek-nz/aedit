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
#  $Id: Makefile 89 2024-02-07 06:00:57Z rhubarb-geek-nz $

CFLAGS=-I. -dHAVE_TMPFILE -dHAVE_TERMIOS_H= -dHAVE_PWD_H= -dHAVE_CFMAKERAW= -d0
SRCFILES=..\src\aedit.c dosvt100.c
HDRFILES=termios.h pwd.h

all: aedit.exe

clean: .SYMBOLIC
	if exist aedit.exe del aedit.exe

aedit.exe: $(SRCFILES) $(HDRFILES)
	wcl.exe -0 -fpc $(SRCFILES) -fe=$@ $(CFLAGS)
	del *.obj
