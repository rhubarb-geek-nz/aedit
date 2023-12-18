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
#  $Id: Makefile 45 2023-12-18 02:15:06Z rhubarb-geek-nz $

all: dist

dist: aedit
	pkg/$$(uname)

aedit: src/aedit.c config.h
	$(CC) $(CFLAGS) -I. -DHAVE_CONFIG_H src/aedit.c -o $@

config.h: configure
	CFLAGS="$(CFLAGS)" ./configure

clean:
	rm -rf aedit config.h *.pkg *.deb *.rpm *.tgz *.txz *.pub
