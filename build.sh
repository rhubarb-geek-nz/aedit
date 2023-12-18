#!/bin/sh -e
#
#  Copyright 2020, Roger Brown
#
#  This file is part of rhubarb pi.
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
# $Id: build.sh 3 2021-01-22 17:52:20Z rhubarb-geek-nz $
#

svnVer()
{
	grep "Id:\ aedit.c" < src/aedit.c | while read A
	do
		LAST=x
		DONE=false
		for d in \\$A
		do
			case "$LAST" in
			aedit.c )
				echo "$d"
				DONE=true
				;;
			* )
				LAST="$d"
				;;
			esac
			if $DONE
			then	
				break
			fi
		done
	done
}

getSize()
{
	du -sk data/usr/bin/aedit | while read A B
	do
		echo $A
	done
}

cleanup()
{
	rm -rf debian-binary control.tar.* data.tar.* data control config.h rpms rpm.spec
}

umask 022

cleanup

trap cleanup 0

VERSION=`svnVer`
HIVERS=`echo $VERSION / 10000 | bc`
LOVERS=`echo $VERSION % 10000 | bc`
PWD=`pwd`

if test "$HIVERS" = ""
then 
	HIVERS=0
fi

if test "$LOVERS" = ""
then 
	LOVERS=0
fi

mkdir -p data/usr/bin control rpms

VERSION="1.1.$HIVERS.$LOVERS"
PKGNAME=aedit

	cat > config.h <<EOF
#define HAVE_CFMAKERAW
#define HAVE_LIMITS_H
#define HAVE_FCNTL_H
#define HAVE_TMPFILE
#define HAVE_PWD_H
#define HAVE_SYS_IOCTL_H
#define HAVE_TERMIOS_H
EOF

if gcc --version
then
	gcc -Wall -Werror -DHAVE_CONFIG_H -I. src/aedit.c -o data/usr/bin/aedit
else
	cc -DHAVE_CONFIG_H -I. src/aedit.c -o data/usr/bin/aedit
fi

ls -ld data/usr/bin/aedit

(
	set -e
	cd data
	if gtar  --owner=0 --group=0 --create --file - *
	then
		:
	else
		tar cf - *
	fi
) > aedit-"$VERSION".tar

ls -ld aedit-"$VERSION".tar
tar tvf aedit-"$VERSION".tar

if dpkg --print-architecture 2>/dev/null
then
	DPKGARCH=`dpkg --print-architecture`

	SIZE=`getSize`

	echo "2.0" > debian-binary

	cat > control/control <<EOF
Package: $PKGNAME
Version: $VERSION
Architecture: $DPKGARCH
Installed-Size: $SIZE
Maintainer: rhubarb-geek-nz@users.sourceforge.net
Section: editors
Priority: extra
Description: Aedit editor
 Text editor based resembling ISIS alter.
 .
EOF

	for d in data control
	do
		(
			set -e
			cd $d
			tar --owner=0 --group=0 --create --xz --file - *
		) > $d.tar.xz
	done

	cat control/control

	ar r "$PKGNAME"_"$VERSION"_"$DPKGARCH".deb debian-binary control.tar.* data.tar.*
fi

if rpmbuild --version 2>/dev/null
then
	cat > rpm.spec <<EOF
Summary: Aedit editor
Name: aedit
Version: $VERSION
Release: 1
Group: Applications/System
License: GPL
Prefix: /usr

%description
Text editor based on ISIS alter

%files
%defattr(-,root,root)
%attr(755,root,root) /usr/bin/aedit

%clean
EOF

	rpmbuild --buildroot "$PWD/data" --define "_rpmdir $PWD/rpms" -bb "$PWD/rpm.spec" 

	find rpms -name "*.rpm" -type f | while read N
	do
		rpm -qlvp "$N"
		mv "$N" .
		basename "$N"
	done
fi
