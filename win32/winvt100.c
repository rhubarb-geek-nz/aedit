/**************************************************************************
 *
 *  Copyright 1989-2012, Roger Brown
 *
 *  This file is part of Roger Brown's Aedit.
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
 * $Id: winvt100.c 45 2023-12-18 02:15:06Z rhubarb-geek-nz $
 */

#ifndef WIN32_LEAN_AND_MEAN
#	define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <stdio.h>
#include <termios.h>

static struct termios console={0,0};
extern void winch(int);

void bomb(int line)
{
	HANDLE h=GetStdHandle(STD_OUTPUT_HANDLE);
	if (h != INVALID_HANDLE_VALUE)
	{
		char buf[256];
		int i=sprintf(buf,"error at line %d of file %s\n",
				line,__FILE__);
		DWORD dw;
		WriteFile(h,buf,i,&dw,NULL);
	}
	ExitProcess(1);
}

int tcgetattr(int fd,struct termios *attr)
{
	HANDLE h;

	if (fd != 0) bomb(__LINE__);

	h=GetStdHandle(STD_INPUT_HANDLE);

	if (h != INVALID_HANDLE_VALUE)
	{
		DWORD mode;
		if (GetConsoleMode(h,&mode))
		{
			console.mode=mode;
		}
	}

	*attr=console;

	return 0;
}

int cfmakeraw(struct termios *attr)
{
	attr->raw=1;
	attr->mode&=~(ENABLE_LINE_INPUT|ENABLE_ECHO_INPUT|ENABLE_PROCESSED_INPUT);
	attr->mode|=ENABLE_WINDOW_INPUT;
	return 0;
}

int tcsetattr(int fd,int how,struct termios *attr)
{
	HANDLE h;
	if (fd != 0) bomb(__LINE__);
	console=*attr;
	(void)how;
	h=GetStdHandle(STD_INPUT_HANDLE);
	if (h != INVALID_HANDLE_VALUE)
	{
		if (!SetConsoleMode(h,attr->mode))
		{
			h=GetStdHandle(STD_OUTPUT_HANDLE);
			if (h != INVALID_HANDLE_VALUE)
			{
				DWORD dw=2;
				unsigned char buf[2];
				buf[0]=0xff;
				buf[1]=(unsigned char)(attr->raw ? 0xfe : 0xfd);
				WriteFile(h,buf,2,&dw,NULL);
			}
		}
	}
	return 0;
}

static char read_buffer[1024];
static int read_len;

int tty_read(int fd,char *buf,int len)
{
	HANDLE h=GetStdHandle(STD_INPUT_HANDLE);

	(void)fd;

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

	if (h != INVALID_HANDLE_VALUE)
	{
		DWORD dw=len;
		INPUT_RECORD input;

		while (ReadConsoleInput(h,&input,1,&dw))
		{
			buf[0]=0;

			switch (input.EventType)
			{
			case WINDOW_BUFFER_SIZE_EVENT:
				winch(0);
				return -1;
			case KEY_EVENT:
				if (input.Event.KeyEvent.bKeyDown)
				{

					if (input.Event.KeyEvent.uChar.AsciiChar)
					{
						switch (input.Event.KeyEvent.uChar.AsciiChar)
						{
						case 27:
							*buf=27;
							read_buffer[read_len++]='[';
							read_buffer[read_len++]='P';
							return 1;
						}

						*buf=input.Event.KeyEvent.uChar.AsciiChar;
						return 1;
					}
					else
					{
						char code=0;
						int arg=0;

						switch (input.Event.KeyEvent.wVirtualKeyCode)
						{
						case VK_ESCAPE:
							code='P';
							break;
						case VK_END:
							code='~'; arg=4;
							break;
						case VK_PRIOR:
							code='~'; arg=5;
							break;
						case VK_NEXT:
							code='~'; arg=6;
							break;
						case VK_HOME:
							code='~'; arg=1;
							break;
						case VK_INSERT:
							code='~'; arg=2;
							break;
						case VK_DELETE:
							code='~'; arg=3;
							break;
						case VK_LEFT:
							code='D';
							break;
						case VK_RIGHT:
							code='C'; 
							break;
						case VK_UP:
							code='A';
							break;
						case VK_DOWN:
							code='B';
							break;
						}

						if (code)
						{
							*buf=27;

							if (arg)
							{
								read_len+=sprintf(read_buffer+read_len,"[%d%c",arg,code);
							}
							else
							{
								read_buffer[read_len++]='[';
								read_buffer[read_len++]=code;
							}

							return 1;
						}
					}
				}

				break;
			}
		}
		
		if (ReadFile(h,buf,len,&dw,NULL))
		{
			return dw;
		}
		return -1;
	}

	return 0;
}

static int escape_len;
static char escape_buffer[256];
static int escape_arg[20];
static int escape_argptr;
static int scroll_first,scroll_last;

void tty_putchar(int c)
{
	HANDLE h=GetStdHandle(STD_OUTPUT_HANDLE);

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
						CONSOLE_SCREEN_BUFFER_INFO info;

						if (GetConsoleScreenBufferInfo(h,&info))
						{
							info.dwCursorPosition.X=0;
							info.dwCursorPosition.Y=0;
							if (escape_arg[0])
							{
								info.dwCursorPosition.Y=(WORD)(escape_arg[0]-1);
							}
							if (escape_arg[1])
							{
								info.dwCursorPosition.X=(WORD)(escape_arg[1]-1);
							}
							if (info.dwCursorPosition.X >
								info.dwSize.X)
							{
								info.dwCursorPosition.X=(WORD)(info.dwSize.X-1);
							}
							if (info.dwCursorPosition.Y >
								info.dwSize.Y)
							{
								info.dwCursorPosition.Y=(WORD)(info.dwSize.Y-1);
							}
							SetConsoleCursorPosition(h,info.dwCursorPosition);
						}

					}
					break;
				case 'n':
					{
						CONSOLE_SCREEN_BUFFER_INFO info;

						if (GetConsoleScreenBufferInfo(h,&info))
						{
							char *p=read_buffer+read_len;
							read_len+=sprintf(p,"\033[%d;%dR",
										info.dwCursorPosition.Y+1,
										info.dwCursorPosition.X+1);
						}
					}
					break;
				case 'J':
					{
						CONSOLE_SCREEN_BUFFER_INFO info;

						if (GetConsoleScreenBufferInfo(h,&info))
						{
							DWORD dw=0;
							switch (escape_arg[0])
							{
							case 2:
								FillConsoleOutputCharacter(h,
									' ',
									info.dwMaximumWindowSize.X*info.dwMaximumWindowSize.Y,
									info.dwCursorPosition,&dw);
								FillConsoleOutputAttribute(h,
									info.wAttributes,
									info.dwMaximumWindowSize.X*info.dwMaximumWindowSize.Y,
									info.dwCursorPosition,&dw);
								break;
							default:
								bomb(__LINE__);
							}
						}
					}
					break;
				case 'K':
					{
						CONSOLE_SCREEN_BUFFER_INFO info;

						if (GetConsoleScreenBufferInfo(h,&info))
						{
							DWORD dw=0;
							DWORD len=(info.dwMaximumWindowSize.X-info.dwCursorPosition.X);
							FillConsoleOutputCharacter(h,
								' ',len,
								info.dwCursorPosition,&dw);
							FillConsoleOutputAttribute(h,
								info.wAttributes,len,
								info.dwCursorPosition,&dw);
							break;
						}
					}
					break;
				case 'm':
					{
						CONSOLE_SCREEN_BUFFER_INFO info;

						if (GetConsoleScreenBufferInfo(h,&info))
						{
							static int reverse=0;
							if (reverse != escape_arg[0])
							{
								WORD w=0;

if (info.wAttributes & FOREGROUND_RED) w|=BACKGROUND_RED;
if (info.wAttributes & FOREGROUND_GREEN) w|=BACKGROUND_GREEN;
if (info.wAttributes & FOREGROUND_BLUE) w|=BACKGROUND_BLUE;
if (info.wAttributes & FOREGROUND_INTENSITY) w|=BACKGROUND_INTENSITY;

if (info.wAttributes & BACKGROUND_RED) w|=FOREGROUND_RED;
if (info.wAttributes & BACKGROUND_GREEN) w|=FOREGROUND_GREEN;
if (info.wAttributes & BACKGROUND_BLUE) w|=FOREGROUND_BLUE;
if (info.wAttributes & BACKGROUND_INTENSITY) w|=FOREGROUND_INTENSITY;

								if (SetConsoleTextAttribute(h,w))
								{
									reverse=escape_arg[0];
								}
								else
								{
									int err=GetLastError();
									if (err)
									{
										bomb(__LINE__);
									}
								}
							}
						}
					}
					break;
				case 'r':
					{
						CONSOLE_SCREEN_BUFFER_INFO info;

						if (GetConsoleScreenBufferInfo(h,&info))
						{
							scroll_first=0;
							scroll_last=info.dwSize.Y-1;
							if (escape_arg[0])
							{
								scroll_first=escape_arg[0]-1;
							}
							if (escape_arg[1])
							{
								scroll_last=escape_arg[1]-1;
							}
						}
					}
					break;
				default:
					bomb(__LINE__);
					break;
				}

				escape_len=0;
				return;
			}
		}

		if (escape_len==1)
		{
			CONSOLE_SCREEN_BUFFER_INFO info;

			if (GetConsoleScreenBufferInfo(h,&info))
			{
				SMALL_RECT scroll;
				SMALL_RECT clip;
				COORD dest;
				CHAR_INFO fill;

				scroll.Left=0;
				scroll.Top=0;
				scroll.Right=(WORD)(info.dwSize.X-1);
				scroll.Bottom=(WORD)(info.dwSize.Y-1);

				clip=scroll;
				dest.X=0;
				dest.Y=0;

				fill.Attributes=info.wAttributes;
				fill.Char.AsciiChar=' ';

				switch (c)
				{
				case '[':
					break;
				case 'E':
					{
						/* used for scrolling up */
						scroll.Top=(WORD)(scroll_first+1);
						scroll.Bottom=(WORD)scroll_last;
						dest.Y=(WORD)(scroll.Top-1);
						ScrollConsoleScreenBuffer(h,
							&scroll,&clip,dest,&fill);

						escape_len=0;

						return;
					}
					break;
				case 'M':
					{
						/* used for scrolling down */
						scroll.Top=(WORD)scroll_first;
						scroll.Bottom=(WORD)(scroll_last-1);
						dest.Y=(WORD)(scroll.Top+1);
						ScrollConsoleScreenBuffer(h,
							&scroll,&clip,dest,&fill);
						escape_len=0;

						return;
					}
					break;
				default:
					bomb(__LINE__);
				}
			}
			else
			{
				bomb(__LINE__);
			}
		}

		escape_buffer[escape_len++]=(char)c;
		escape_buffer[0]=0;
	}
	else
	{
		if (h!=INVALID_HANDLE_VALUE)
		{
			DWORD mode;
			DWORD dw=1;
			char b=(char)c;
			if (GetConsoleMode(h,&mode))
			{
				if (b==27)
				{
					escape_buffer[escape_len++]=b;
					escape_buffer[escape_len]=0;
					escape_argptr=20;
					while (escape_argptr)
					{
						escape_arg[--escape_argptr]=0;
					}
				}
				else
				{
					WriteFile(h,&b,1,&dw,NULL);
				}
			}
			else
			{
				WriteFile(h,&b,1,&dw,NULL);
			}
		}
	}
}

int tty_winsize(int *cols,int *rows)
{
	HANDLE h=GetStdHandle(STD_OUTPUT_HANDLE);

	if (h != INVALID_HANDLE_VALUE)
	{
		CONSOLE_SCREEN_BUFFER_INFO info;

		if (GetConsoleScreenBufferInfo(h,&info))
		{
			*cols=info.dwMaximumWindowSize.X;
			*rows=info.dwMaximumWindowSize.Y;
			return 0;
		}
	}

	return -1;
}
