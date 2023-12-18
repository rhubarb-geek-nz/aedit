#
#  Copyright 2022, Roger Brown
#
#  This file is part of Roger Brown's Aedit.
#
#  This program is free software: you can redistribute it and/or modify it
#  under the terms of the GNU Lesser General Public License as published by the
#  Free Software Foundation, either version 3 of the License, or (at your
#  option) any later version.
# 
#  This program is distributed in the hope that it will be useful, but WITHOUT
#  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
#  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
#  more details.
#
#  You should have received a copy of the GNU Lesser General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>
#
#  $Id: Makefile 28 2022-11-26 09:18:27Z rhubarb-geek-nz $

SRC=..\src\aedit.c winvt100.c tmpfile.c
CL=cl
OBJDIR=obj\$(VSCMD_ARG_TGT_ARCH)
BINDIR=bin\$(VSCMD_ARG_TGT_ARCH)
APP=$(BINDIR)\aedit.exe

all: $(APP)
	
clean: 
	if exist $(APP) del $(APP)
	if exist $(OBJDIR)\*.obj del $(OBJDIR)\*.obj
	if exist $(OBJDIR) rmdir $(OBJDIR)
	if exist $(BINDIR) rmdir $(BINDIR)

$(APP): $(SRC) $(OBJDIR) $(BINDIR)
	$(CL) 							\
		/Fe$@ 						\
		/Fo$(OBJDIR)\					\
		/W3 							\
		/WX 							\
		/MT 							\
		/I.							\
		/DNDEBUG 						\
		/D_CRT_SECURE_NO_DEPRECATE 			\
		/D_CRT_NONSTDC_NO_DEPRECATE  			\
		/DHAVE_LIMITS_H					\
		/DHAVE_FCNTL_H					\
		/DWIN32_LEAN_AND_MEAN				\
		$(SRC) 						\
		/link							\
		/INCREMENTAL:NO					\
		/PDB:NONE						\
		/SUBSYSTEM:CONSOLE
	dir $@

$(OBJDIR) $(BINDIR): 
	mkdir $@
