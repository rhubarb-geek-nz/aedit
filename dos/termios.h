/**************************************************************************
 *
 *  Copyright 1989-2012, Roger Brown
 *
 *  This file is part of Roger Brown's Toolkit.
 *
 *  This program is free software: you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation, either version 3 of the License, or (at your
 *  option) any later version.
 * 
 *  This program is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 *  more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>
 *
 */

/*
 * $Id: termios.h 90 2024-02-07 20:49:10Z rhubarb-geek-nz $
 */

#ifndef __TERMIOS_H__
#define __TERMIOS_H__

extern int total_lines;
extern int total_cols;

struct termios
{
	unsigned char c_cc[8];
};

#define tcgetattr(a,b)
#define cfmakeraw(a)

#define TCSANOW   1
#define TCSADRAIN 2

#define VMIN      4
#define VTIME     5

#undef getchar
#undef putchar

#define getchar   tty_getchar
#define putchar   tty_putchar
#define read      termios_read

extern int tty_getchar(void);
extern void tty_putchar(int);
extern int read(int,char *,int);
extern int tty_winsize(int *,int *);
extern int tcsetattr(int,int,const struct termios *);

extern int termios_system(void);
#define system(x) termios_system()

#if defined(__OS2__) || defined(_WIN32)
#	define	TIOCGWINSZ		1
struct winsize { int ws_row,ws_col; };
#define ioctl(a,b,c)	tty_ioctl(a,b,c)
extern int tty_ioctl(int,int,struct winsize *);
#endif

#ifdef __OS2__
#	undef SCROLL_RGN
#endif

#ifdef _WIN32
#	undef _WIN32
#endif

#endif
