/**************************************************************************
 *
 *  Copyright 1989-2012, Roger Brown
 *
 *  This file is part of Roger Brown's Toolkit.
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
 * $Id: termios.h 33 2023-12-17 01:30:12Z rhubarb-geek-nz $
 */

struct termios
{
	int raw;
	unsigned long mode;
	unsigned char c_cc[8];
};

#define HAVE_CFMAKERAW

extern int tcgetattr(int fd,struct termios *ios);
extern int cfmakeraw(struct termios *ios);
extern int tcsetattr(int fd,int when,struct termios *ios);

#define TCSANOW   1
#define TCSADRAIN 2

#define VMIN      4
#define VTIME     5

#undef getchar
#undef putchar

#define getchar  tty_getchar
#define putchar  tty_putchar

int tty_getchar(void);
void tty_putchar(int);
int tty_kbhit(void);
int tty_read(int,char *,int);

int tty_winsize(int *cols,int *rows);