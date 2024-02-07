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
 * $Id: dosvt100.c 90 2024-02-07 20:49:10Z rhubarb-geek-nz $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <conio.h>
#include <direct.h>
#ifdef _WIN32
#	define WIN32_LEAN_AND_MEAN
#	include <windows.h>
#else
#	ifdef __OS2__
#		define INCL_VIO
#		include <os2.h>
#	else
#		include <graph.h>
#	endif
#endif
#include <termios.h>
#include <pwd.h>

#undef getchar
#undef system
#undef putchar

int tty_winsize(int *cols,int *rows)
{
#ifdef  _GRAPH_H_INCLUDED
	struct videoconfig videoconfig, __far * vc=_getvideoconfig(&videoconfig);

	if (vc)
	{
		*rows=vc->numtextrows;
		*cols=vc->numtextcols;
	}
	else
#else
#	ifdef __OS2__
	VIOMODEINFO info;

	memset(&info,0,sizeof(info));
	info.cb=sizeof(info);

	if (!VioGetMode(&info,0))
	{
		*cols=info.col;
		*rows=info.row;
	}
	else
#	else
	CONSOLE_SCREEN_BUFFER_INFO info;
	HANDLE h=GetStdHandle(STD_OUTPUT_HANDLE);

	if (h != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(h,&info))
	{
		*cols=info.dwMaximumWindowSize.X;
		*rows=info.dwMaximumWindowSize.Y;
	}
	else
#	endif
#endif
	{
		*cols=80;
		*rows=25;
	}

	return 0;
}

int tcsetattr(int fd, int optional_actions,const struct termios *termios_p)
{
	return tty_winsize(&total_cols,&total_lines);
}

static char read_buffer[32];
static int read_len;

int termios_read(int fd,char *buf,int len)
{
	if (!read_len)
	{
		int ch=-1;

		while ((ch=getch())!=-1)
		{
			int code=0,arg=0;

#ifdef __OS2__
			if (ch != 0xE0)
#else
			if (ch)
#endif
			{
				read_buffer[read_len++]=ch;
				break;
			}

			ch=getch();

			switch (ch)
			{
				case 79:
					code='~'; arg=4;
					break;
				case 73:
					code='~'; arg=5;
					break;
				case 81:
					code='~'; arg=6;
					break;
				case 71:
					code='~'; arg=1;
					break;
				case 82:
					code='~'; arg=2;
					break;
				case 83:
					code='~'; arg=3;
					break;
				case 75:
					code='D';
					break;
				case 77:
					code='C'; 
					break;
				case 72:
					code='A';
					break;
				case 80:
					code='B';
					break;
			}

			if (code)
			{
				read_buffer[read_len++]=27;

				if (arg)
				{
					read_len+=sprintf(read_buffer+read_len,"[%d%c",arg,code);
				}
				else
				{
					read_buffer[read_len++]='[';
					read_buffer[read_len++]=code;
				}

				break;
			}
		}
	}

	if (read_len)
	{
		int i=read_len;
		if (i > len)
		{
			i=len;
		}
		memcpy(buf,read_buffer,i);
		read_len-=i;
		if (read_len)
		{
			memmove(read_buffer,read_buffer+i,read_len);
		}
		return i;
	}

	return 0;
}

static struct passwd static_passwd={":*:"};

struct passwd *getpwuid(int uid)
{
	return &static_passwd;
}

int termios_system(void)
{
	char *p=getenv("COMSPEC");
	int result=-1;

	if (p)
	{
		int drive=_getdrive();

		if (drive>=0)
		{
			char cwd[PATH_MAX];

			if (getcwd(cwd,sizeof(cwd)))
			{
				result=system(p);

				_chdrive(drive);
				chdir(cwd);
			}
		}
	}

	return result;
}

static int escape_len;
static char escape_buffer[256];
static int escape_arg[20];
static int escape_argptr;
static int scroll_first,scroll_last;
static char blank_line[256];

void tty_putchar(int c)
{
#ifdef  _GRAPH_H_INCLUDED
	if (escape_len)
	{
		if (escape_len==2)
		{
			if (escape_buffer[1]=='[')
			{
				if (c==';')
				{
					escape_argptr++;
					return;
				}
				if ((c>='0')&&(c<='9'))
				{
					int i=c-'0';
					escape_arg[escape_argptr]*=10;
					escape_arg[escape_argptr]+=i;
					return;
				}
				switch (c)
				{
				case 'H':
					{
						struct rccoord info;
						struct videoconfig videoconfig, __far * vc=_getvideoconfig(&videoconfig);
						info.row=1;
						info.col=1;

						if (escape_arg[0])
						{
							info.row=(short)(escape_arg[0]);
						}

						if (escape_arg[1])
						{
							info.col=(short)(escape_arg[1]);
						}

						if (vc)
						{
							if (info.row > vc->numtextrows) info.row=vc->numtextrows;
							if (info.col > vc->numtextcols) info.col=vc->numtextcols;
						}

						_settextposition(info.row,info.col);
					}
					break;
				case 'n':
					{
						struct rccoord info=_gettextposition();
						char *p=read_buffer+read_len;
						read_len+=sprintf(p,"\033[%d;%dR",
								info.row,
								info.col);
					}
					break;
				case 'J':
					{
						struct rccoord info=_gettextposition();
						struct videoconfig videoconfig, __far * vc=_getvideoconfig(&videoconfig);
						if (vc)
						{
							switch (escape_arg[0])
							{
							case 2:
								{
									int row=0;
									memset(blank_line,' ',vc->numtextcols);

									while (row++ < vc->numtextrows)
									{
										_settextposition(row,1);
										_outmem(blank_line,vc->numtextcols);
									}
								
									_settextposition(info.row,info.col);
								}
								break;
							}
						}
					}
					break;
				case 'K':
					{
						struct rccoord info=_gettextposition();
						struct videoconfig videoconfig, __far * vc=_getvideoconfig(&videoconfig);
						if (vc)
						{
							int cols=1+(vc->numtextcols-info.col);

							if (info.row==vc->numtextrows)
							{
								cols--;
							}

							if (cols > 0)
							{
								memset(blank_line,' ',cols);

								_outmem(blank_line,cols);
								
								_settextposition(info.row,info.col);
							}
						}
					}
					break;
				case 'm':
					{
						static int reverse=0;
						if (reverse != escape_arg[0])
						{
							short color=_gettextcolor();
							short bkcolor=_getbkcolor();

							_settextcolor(bkcolor);
							_setbkcolor(color);

							reverse=escape_arg[0];
						}
					}
					break;
				case 'r':
					{
						struct videoconfig videoconfig, __far * vc=_getvideoconfig(&videoconfig);
						scroll_first=1;
						scroll_last=vc->numtextrows;
						if (escape_arg[0])
						{
							scroll_first=escape_arg[0];
						}
						if (escape_arg[1])
						{
							scroll_last=escape_arg[1];
						}
					}
					break;
				}

				escape_len=0;
				return;
			}
		}

		if (escape_len==1)
		{
			struct rccoord info=_gettextposition();
			struct videoconfig videoconfig, __far * vc=_getvideoconfig(&videoconfig);

			switch (c)
			{
			case '[':
				break;
			case 'E':
				{
					_settextwindow(scroll_first,1,scroll_last,vc->numtextcols);
					_scrolltextwindow(_GSCROLLUP);
					_settextwindow(1,1,vc->numtextrows,vc->numtextcols);

					escape_len=0;

					return;
				}
				break;
			case 'M':
				{
					_settextwindow(scroll_first,1,scroll_last,vc->numtextcols);
					_scrolltextwindow(_GSCROLLDOWN);
					_settextwindow(1,1,vc->numtextrows,vc->numtextcols);

					escape_len=0;

					return;
				}
				break;
			}
		}

		escape_buffer[escape_len++]=(char)c;
		escape_buffer[0]=0;
	}
	else
	{
		if (c==27)
		{
			escape_buffer[escape_len++] = c;
			escape_buffer[escape_len]=0;
			escape_argptr=20;
			while (escape_argptr)
			{
				escape_arg[--escape_argptr]=0;
			}
		}
		else
		{
			char buf[2]={0,0};
			buf[0]=c;
			_outtext(buf);
		}
	}
#else
	putchar(c);
#endif
}

#ifdef TIOCGWINSZ
int tty_ioctl(int fd,int i,struct winsize *ws)
{
	return tty_winsize(&ws->ws_col,&ws->ws_row);
}
#endif

