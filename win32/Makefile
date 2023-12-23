#
#  Copyright 2022, Roger Brown
#
#  This file is part of Roger Brown's Aedit.
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
#  $Id: Makefile 57 2023-12-23 02:59:14Z rhubarb-geek-nz $

SRC=..\src\aedit.c winvt100.c
CL=cl
OBJDIR=obj\$(VSCMD_ARG_TGT_ARCH)
BINDIR=bin\$(VSCMD_ARG_TGT_ARCH)
BUNDLE=bundle
RESFILE=$(OBJDIR)\aedit.res
APP=$(BINDIR)\aedit.exe
MSI=aedit-$(DEPVERS_aedit_STR4)-$(VSCMD_ARG_TGT_ARCH).msi
MSIX=$(BUNDLE)\aedit-$(DEPVERS_aedit_STR4)-$(VSCMD_ARG_TGT_ARCH).msix
MSIXBUNDLE=aedit-$(DEPVERS_aedit_STR4).msixbundle
CHM=htmlhelp\aedit.chm

all: $(APP) $(MSI) $(MSIX)
	
clean: 
	if exist $(APP) del $(APP)
	if exist $(CHM) del $(CHM)
	if exist $(OBJDIR)\*.obj del $(OBJDIR)\*.obj
	if exist $(OBJDIR) rmdir $(OBJDIR)
	if exist $(BINDIR) rmdir $(BINDIR)

$(APP): $(SRC) $(OBJDIR) $(BINDIR) $(RESFILE) termios.h
	$(CL) 							\
		/Fe$@ 						\
		/Fo$(OBJDIR)\				\
		/W3 						\
		/WX 						\
		/MT 						\
		/I.							\
		/DNDEBUG 					\
		/D_CRT_SECURE_NO_DEPRECATE 	\
		/D_CRT_NONSTDC_NO_DEPRECATE \
		/DHAVE_LIMITS_H				\
		/DHAVE_FCNTL_H				\
		/DWIN32_LEAN_AND_MEAN		\
		$(SRC) 						\
		/link						\
		/INCREMENTAL:NO				\
		/PDB:NONE					\
		/SUBSYSTEM:CONSOLE			\
		/DEF:aedit.def				\
		$(RESFILE)
	del "$(BINDIR)\aedit.exp"
	del "$(BINDIR)\aedit.lib"
	signtool sign /sha1 "$(CertificateThumbprint)" /fd SHA256 /t http://timestamp.digicert.com $@

$(RESFILE): aedit.rc
	rc /r $(RCFLAGS) "/DDEPVERS_aedit_INT4=$(DEPVERS_aedit_INT4)" "/DDEPVERS_aedit_STR4=\"$(DEPVERS_aedit_STR4)\"" /fo$@ aedit.rc

$(OBJDIR) $(BINDIR) $(BUNDLE):
	mkdir $@

$(MSI): $(APP) $(CHM)
	"$(WIX)bin\candle.exe" -nologo "wix\$(VSCMD_ARG_TGT_ARCH).wxs" -dDEPVERS_aedit_STR4=$(DEPVERS_aedit_STR4)
	"$(WIX)bin\light.exe" -nologo -cultures:null -out $@ "$(VSCMD_ARG_TGT_ARCH).wixobj"
	del "$(VSCMD_ARG_TGT_ARCH).wixobj"
	del "aedit-$(DEPVERS_aedit_STR4)-$(VSCMD_ARG_TGT_ARCH).wixpdb"
	signtool sign /sha1 "$(CertificateThumbprint)" /fd SHA256 /t http://timestamp.digicert.com $@

$(MSIX): $(BUNDLE) $(APP)
	COPY /Y $(APP)
	DIR aedit.exe
	makeappx pack /m AppxManifest.xml /f mapping.ini /p $@
	DEL aedit.exe
	signtool sign /sha1 "$(BundleThumbprint)" /fd SHA256 /t http://timestamp.digicert.com $@

$(MSIXBUNDLE):
	makeappx bundle /d $(BUNDLE) /p $@
	signtool sign /sha1 "$(BundleThumbprint)" /fd SHA256 /t http://timestamp.digicert.com $@

$(CHM): htmlhelp\aedit.hhp
	cd htmlhelp
	-"$(COMSPEC)" /C "%%ProgramFiles(x86)%%\\HTML Help Workshop\hhc.exe" aedit.hhp
	cd ..
	dir $@
