/**************************************************************************
 *
 *  Copyright 1989-2012, Roger Brown
 *
 *  This file is part of Roger Brown's Aedit.
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
 * $Id: aedit.c 40 2023-12-17 13:19:35Z rhubarb-geek-nz $
 */

/*
 * a simple full screen text editor using standard as possible C IO
 * and minumum CRT codes, can use either VT52 codes (like Intel) or ANSI
 * if terminal can use the plot function then define PLOT_CTRL
 * if terminal can use the erase line function then define ERASE_LINE
 * needs to be able to change stdin to raw mode
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#ifdef HAVE_LIMITS_H
#	include <limits.h>
#endif

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef HAVE_FCNTL_H
#	include <fcntl.h>
#endif

#ifdef HAVE_TMPFILE
#	define TEMP_FP
#endif

#ifdef _WIN32
#	include <io.h>
#	include <direct.h>
#	define TEMP_FP
#	define tmpfile aedit_tmpfile
extern FILE *tmpfile(void);
#else /* _WIN32 */
#	include <unistd.h>
#	define HAVE_PWD_H
#endif /* _WIN32 */

#ifdef HAVE_PWD_H
#	include <pwd.h>
#endif

#ifdef HAVE_SYS_IOCTL_H
#	include <sys/ioctl.h>
#endif

#ifndef lint
char sig[]="@(#)aedit 2.0";
#endif

#ifdef _DEBUG
	#undef _DEBUG
#	define printf  do not use
#endif

#ifdef _DEBUG
#	include <aeditdbg.h>
#else
#	define ADBG_MATCH(posn,cx)
#	define ADBG_DEL(len)
#	define ADBG_INS(ch)
#	define ADBG_MOVE(by)
#	define ADBG_MINS(p,l)
#	define ADBG_POS(posn)
#	define ADBG_LEN(len)
#endif

#ifdef _WIN32
#	define HAVE_TERMIOS_H
#endif

#define PLOT_CTRL
#define ANSI_SYS
#define CLEAR_SCREEN
#define ERASE_LINE

#ifdef ANSI_SYS
#	define REVERSE
#	define SCROLL_RGN
#endif

#ifdef HAVE_TERMIOS_H
#	include <termios.h>
#else /* HAVE_TERMIOS_ */
#	include <sgtty.h>
#endif /* HAVE_TERMIOS */

#ifndef SEEK_SET
#	define SEEK_SET    0
#	define SEEK_CUR    1
#	define SEEK_END	2
	long lseek();
#endif

#ifndef PATH_MAX
#	define PATH_MAX	80
#endif

#define escape		'\033'
#define nl		'\n'
#define tab		'\t'

#define max_lines	256
#define max_cols	256

#define last_text	(total_lines-3)
#define status_line (total_lines-2)
#define menu_line 	(total_lines-1)
#define last_col	(total_cols-1)

#define key_start	0xA0
#define key_up		0xA1
#define page_up		0xA2
#define key_left	0xA4
#define key_home	0xA5
#define key_right	0xA6
#define key_end		0xA8
#define key_down	0xA9
#define page_down	0xAA
#define key_ins		0xAB
#define key_del		0xAC
#define key_find	0xAD
#define key_sel		0xAE

#define ctrl_a		1
#define ctrl_c		3
#define ctrl_d		4
#define ctrl_e		5
#define ctrl_f		6
#define ctrl_n		14
#define ctrl_p		16
#define ctrl_q		17
#define ctrl_r		18
#define ctrl_s		19
#define ctrl_u		21
#define ctrl_w		23
#define ctrl_x		24
#define ctrl_z		26

static int forever=1;
static char f_string[max_cols];
static char r_string[max_cols];
static int last_cmd;
static char get_fname[PATH_MAX];

static void find_cursor(void);
static void ed_dump(long);
static int ed_chr(long);
static int ed_reserve(long);

static int getkey(void);

#ifdef _WIN32
#else /* _WIN32 */
#define tty_read(x,y,z)   read(x,y,z)
#endif /* _WIN32 */

int winched;
void winch(int i)
{
	winched++;		/* acknowledge */
	if (i)
	{
		signal(i,winch);	/* to let it refire */
	}
}

#ifdef HAVE_SIGINTERRUPT
#	define aedit_siginterrupt(n,f) 	siginterrupt(n,f)
#else
static int aedit_siginterrupt(int n,int flag)
{
#ifdef SA_RESTART
	struct sigaction sa;

	sigaction(n,NULL,&sa);

	if (flag)
	{
		sa.sa_flags&=~SA_RESTART;
	}
	else
	{
		sa.sa_flags|=SA_RESTART;
	}

	sigaction(n,&sa,NULL);
#else
#	ifdef SIG_UNBLOCK
	sigset_t s;
	sigemptyset(&s);
	sigaddset(&s,n);
	sigprocmask(
		(flag ? SIG_UNBLOCK : SIG_BLOCK),
		&s,NULL);
#	endif
#endif

	return 0;
}
#endif

static char *strjoin(char *fmt,...)
{
	size_t len=0;
	int count=0;
	char *retval=NULL;
	size_t joint=strlen(fmt);

	va_list ap;

	va_start(ap,fmt);

	while (1)
	{
		char *p=va_arg(ap,char *);

		if (!p) break;

		count++;
		if (len) len+=joint;
		len+=strlen(p);
	}

	va_end(ap);

	if (count)
	{
		retval=malloc(len+1);
		len=0;

		va_start(ap,fmt);

		while (count--)
		{
			char *p=va_arg(ap,char *);
			size_t l=strlen(p);
			if (len)
			{
				memcpy(retval+len,fmt,joint);
				len+=joint;
			}
			if (l)
			{
				memcpy(retval+len,p,l);
				len+=l;
			}			
		}

		va_end(ap);

		retval[len]=0;
	}

	return retval;
}

static int co_getch(void)
{
	while (forever)
	{
		char buf[1];
		int i;
#ifdef SIGTTIN
		aedit_siginterrupt(SIGWINCH,1);
#endif
		i=tty_read(0,buf,1);
#ifdef SIGTTIN
		aedit_siginterrupt(SIGWINCH,0);
#endif
		if (i==1) 
		{
			if (buf[0] != ctrl_s)
			{
				return (0xff & (int)buf[0]);
			}

			while (tty_read(0,buf,1)==1)
			{
				if (buf[0]==ctrl_q)
				{
					break;
				}
			}
		}
		else
		{
			break;
		}
	}

	return -1;
}

#if defined(HAVE_TERMIOS_H) && !defined(HAVE_CFMAKERAW)
static void cfmakeraw(struct termios *tt)
{
	tt->c_iflag=0;
	tt->c_oflag&=~OPOST;
	tt->c_lflag&=~(ISIG|ICANON|ECHO
#ifdef XCASE
			|XCASE
#endif
			);
	tt->c_cflag&=~(CSIZE|PARENB);
	tt->c_cflag|=CS8;
	tt->c_cc[VMIN]=1;
	tt->c_cc[VTIME]=1;
}
#endif

int guessing;

#ifdef QNX
int stdin_option;
#else /* QNX */
#	ifdef HAVE_TERMIOS_H
static struct termios tty_mode;
#	else /* HAVE_TERMIOS_H */
static long old_flags;
struct sgttyb temp_mode;
#	endif /* HAVE_TERMIOS_H */
#endif /* QNX */


#ifdef HAVE_TERMIOS_H
#	ifndef MALLOC_SIZE
#		ifdef _DEBUG
#			define MALLOC_SIZE 8192
#		else
#			define MALLOC_SIZE 32768
#		endif
#	endif
#endif

int total_lines=24;
int total_cols=80;

char wordwrap=0;
long again=0;
char rev_menu=1;
int tabs=4;

int row,col;

int crsr_row,crsr_col,tty_row,tty_col;
long sel_pos=0;
int sel_on=0,rev_on=0;
int mode=0,menu_id=0,menu_nxt=0,menu_erased=0;
int show_size=0;

/*
 * 0----page_start----low_size . . high_start----buf_size
 * changed -  high_size is length of highpart below buf_size
 *
 */

long page_start;

static char *filename;
static int chars_per_line[max_lines];
static char *clip_name="clipbrd.tmp";

#ifndef MALLOC_SIZE
static char curfbuf[8192];
#endif

struct ed_file
{
	/* in memory buffer */
	long buf_size;
	char *buffer;
	long low_size,high_size;
	/* this is the temporary file */
#ifdef TEMP_FP
	FILE *t_fp;
#else
	int t_fd;
#endif
	/* temp file stats */
	long t_lol; /* temp low length */
	long t_hip; /* temp high offset */
	long t_hil; /* temp high length */
	long t_capos; /* cache position */
	long t_calen; /* cache length */
	char t_cache[512];
} cur_file={
#ifdef MALLOC_SIZE
	MALLOC_SIZE,0,
#else
	sizeof(curfbuf),curfbuf,
#endif
	0,0,
#ifdef TEMP_FP
	NULL,
#else
	-1,
#endif
	0,0,0,0,0
};

#ifdef TEMP_FP
#else
char tmpname[1024];
#endif

static int tty_raw(int fd)
{
#	ifdef QNX
	set_option(stdin,((stdin_option=get_option(stdin))&~(EDIT|ECHO)));
#	else /* QNX */
#		ifdef HAVE_TERMIOS_H
	struct termios temp_mode;
	tcgetattr(fd,&temp_mode);
	tty_mode=temp_mode;
	cfmakeraw(&temp_mode);
    temp_mode.c_cc[VMIN]=1;
    temp_mode.c_cc[VTIME]=1;
	return tcsetattr(fd,TCSADRAIN /*TCSANOW*/,&temp_mode);
#		else /* HAVE_TERMIOS_H */
    ioctl(fd,TIOCGETP,(char *)&temp_mode);
	old_flags=temp_mode.sg_flags;
	temp_mode.sg_flags&=~ECHO;
	temp_mode.sg_flags|=RAW;
	return ioctl(fd,TIOCSETP,(char *)&temp_mode);
#		endif /* HAVE_TERMIOS_H */
#	endif /* QNX */
}

static int tty_reset(int fd)
{
#ifdef QNX
	return set_option(stdin,stdin_option);
#else /* QNX */
#ifdef HAVE_TERMIOS_H
	return 	tcsetattr(fd,TCSADRAIN /*TCSANOW*/,&tty_mode);
#else /* HAVE_TERMIOS_H */
    temp_mode.sg_flags=old_flags;
    return ioctl(fd,TIOCSETP,(char *)&temp_mode);	
#endif /* HAVE_TERMIOS_H */
#endif /* QNX */
}

static long ed_len(void)
{
	return cur_file.low_size+cur_file.high_size+cur_file.t_lol+cur_file.t_hil;
}

static long ed_room(void)
{
	return cur_file.buf_size-cur_file.low_size-cur_file.high_size;
}

static void ed_seek(long p)
{
	long q;
#ifdef TEMP_FP
	fseek(cur_file.t_fp,p,SEEK_SET);
	q=ftell(cur_file.t_fp);
#else
	q=lseek(cur_file.t_fd,p,SEEK_SET);
#endif
	if (q != p)
	{
		perror("seek");
/*		printf("q=%ld, p=%ld\n",q,p);*/
		tty_reset(0);
		exit(1);
	}
}

static void ed_read(char *p,long l)
{
	size_t len=l;

	while (len)
	{
#ifdef TEMP_FP
		size_t i=fread(p,1,len,cur_file.t_fp);
#else
		int i=read(cur_file.t_fd,p,len);
#endif
		if (i > 0)
		{
			len-=i;
			p+=i;
		}
		else
		{
			perror("read");
			tty_reset(0);
			exit(1);
		}
	}
}

static void ed_tzap(void)
{
	cur_file.t_capos=0;
	cur_file.t_calen=0;

#ifdef _DEBUG
/*	printf("ed_tzap\n");*/
#endif
}

static void ed_write(char *p,long l)
{
	size_t len=l;

	if (len)
	{
		ed_tzap();
	}

	while (len)
	{
#ifdef TEMP_FP
		size_t i=fwrite(p,1,len,cur_file.t_fp);
#else
		int i=write(cur_file.t_fd,p,len);
#endif
		if (i > 0)
		{
			len-=i;
			p+=i;
		}
		else
		{
			perror("write");
			tty_reset(0);
			exit(1);
		}
	}

#ifdef TEMP_FP
	fflush(cur_file.t_fp);
#endif
}

static void ed_fclr(long pos,long len)
{
	char buf[256];
	char *p=buf;
	int i=sizeof(buf);
	while (i--) { *p=0; p++; }
	ed_seek(pos);
	while (len)
	{
		if (len > sizeof(buf))
		{
			ed_write(buf,sizeof(buf));
			len-=(sizeof(buf));
		}
		else
		{
			ed_write(buf,len);
			len=0;
		}
	}
}

static void ed_load(long l)
{
	if (l)
	{
		if (l > 0)
		{
			if (l > ed_room())
			{
				l=ed_room();
			}

			if (l > cur_file.t_hil)
			{
				l=cur_file.t_hil;
			}

			if (l)
			{
				long i=cur_file.high_size;
				char *src=cur_file.buffer+cur_file.buf_size-cur_file.high_size;
				char *tgt=src-l;

#ifdef _DEBUG
				printf("ed_load(%ld) above\n",l);
#endif

				while (i--)
				{
					*tgt++=(*src++);
				}

				ed_seek(cur_file.t_hip);
				ed_read(tgt,l);
				ed_fclr(cur_file.t_hip,l);

				cur_file.high_size+=l;
				cur_file.t_hip+=l;
				cur_file.t_hil-=l;
			}
		}
		else
		{
			l=(-l);

			if (l > ed_room())
			{
				l=ed_room();
			}

			if (l > cur_file.t_lol)
			{
				l=cur_file.t_lol;
			}

			if (l)
			{
				long i=cur_file.low_size;
				char *src=cur_file.buffer+cur_file.low_size;
				char *tgt=src+l;

#ifdef _DEBUG
				printf("ed_load(%ld) below\n",l);
#endif

				while (i--)
				{
					*--tgt=(*--src);
				}

				ed_seek(cur_file.t_lol-l);
				ed_read(cur_file.buffer,l);

				ed_fclr(cur_file.t_lol-l,l);

				cur_file.t_lol-=l;
				cur_file.low_size+=l;
			}
		}
	}
}

static int ed_at(long pos)
{
	long off=pos;
	int c=EOF;

#if defined(_WIN32) && defined(_M_IX86)
	if (off < 0)
	{
		__asm int 3;
	}

	if (off >= ed_len())
	{
		__asm int 3;
	}
#endif
	
	if (off < cur_file.t_lol)
	{
		c=ed_chr(off);
		ADBG_MATCH(pos,c)
	}
	else
	{
		off-=cur_file.t_lol;

		if (off < cur_file.low_size)
		{
			c=cur_file.buffer[off];
			ADBG_MATCH(pos,c)
		}
		else
		{
			off-=cur_file.low_size;

			if (off < cur_file.high_size)
			{
				off+=cur_file.buf_size;
				off-=cur_file.high_size;
				c=cur_file.buffer[off];

				ADBG_MATCH(pos,c)
			}
			else
			{
				off-=cur_file.high_size;

				if (off < cur_file.t_hil)
				{
					off+=cur_file.t_hip;
	
					c=ed_chr(off);

					ADBG_MATCH(pos,c)
				}
				else
				{
#if defined(_WIN32) && defined(_M_IX86)
					__asm int 3;
#endif
				}
			}
		}
	}

	ADBG_MATCH(pos,c)

	return c;
}

static int ed_chr(long off)
{
	long mask=sizeof(cur_file.t_cache);
	mask--;
	mask=(~mask);

	do
	{
		if (off >= cur_file.t_capos)
		{
			if (off < (cur_file.t_capos+cur_file.t_calen))
			{
				return cur_file.t_cache[off-cur_file.t_capos];
			}
		}

		cur_file.t_capos=(mask & off);
		ed_seek(cur_file.t_capos);

#ifdef _DEBUG
		printf("read cache at %ld\n",cur_file.t_capos);
#endif

#ifdef TEMP_FP
		cur_file.t_calen=(long)fread(cur_file.t_cache,1,
			sizeof(cur_file.t_cache),
			cur_file.t_fp);
#else
		cur_file.t_calen=read(cur_file.t_fd,cur_file.t_cache,sizeof(cur_file.t_cache));
#endif

	} while (cur_file.t_calen > 0);

	return EOF;
}

static void ed_del(long hwmany)
{
	ADBG_DEL(hwmany)

	ed_tzap();

	/* delete number of chars */
	if (hwmany)
	{
		if (hwmany > 0)
		{
			/* delete forward characters */

			if (hwmany > cur_file.high_size)
			{
				hwmany-=cur_file.high_size;
				cur_file.high_size=0;
				cur_file.t_hip+=hwmany;
				cur_file.t_hil-=hwmany;
			}
			else
			{
				cur_file.high_size-=hwmany;
			}
		}
		else
		{
			hwmany=(-hwmany);

			if (hwmany > cur_file.low_size)
			{
				hwmany-=cur_file.low_size;
				cur_file.low_size=0;
				cur_file.t_lol-=hwmany;
			}
			else
			{
				cur_file.low_size-=hwmany;
			}

			/* delete backward characters */
		}
	}
}

static void ed_ins(int ch)
{
	/* add character at cursor position */

	ADBG_INS(ch)

	ed_tzap();

	while (forever)
	{
		if ((cur_file.low_size+cur_file.high_size) < cur_file.buf_size)
		{
			cur_file.buffer[cur_file.low_size++]=(char)ch;
			break;
		}
		else
		{
			long l=1;
			if (ed_reserve(l))
			{
#if defined(_WIN32) && defined(_M_IX86)
				__asm int 3;
#endif
			}

			/* try and keep mid buffer */
			if (cur_file.high_size > cur_file.low_size)
			{
				l=cur_file.high_size;
				l>>=1;
				ed_dump(l);
			}
			else
			{
				l=cur_file.low_size;
				l>>=1;
				ed_dump(-l);
			}
		}
	}
}

static void ed_dump(long p)
{
	/* move bytes out of buffer into file,
		-v means low part, +ve means high part */

	ed_tzap();

	if (p)
	{
		char *p1,*p2;
		long l;

		if (p > 0)
		{
#ifdef _DEBUG
			printf("dumping %ld from top\n",p);
#endif
			/* move buffer to top half file */

			if (p > cur_file.high_size)
			{
				p=cur_file.high_size;
			}

			if (p)
			{
				ed_reserve(p);

#ifdef _DEBUG
				if ((cur_file.t_lol+p)>cur_file.t_hip)
				{
					/* not enough room in temp */
#if defined(_WIN32) && defined(_M_IX86)
					__asm int 3;
#endif
				}

				printf("room is %ld\n",cur_file.t_hip-cur_file.t_lol);
				printf("actually dumping %ld\n",p);
				printf("high size is %ld\n",cur_file.high_size);
#endif
				cur_file.t_hip-=p;
				cur_file.t_hil+=p;

				ed_seek(cur_file.t_hip);

				p1=cur_file.buffer;
				p1+=cur_file.buf_size;
				p1-=p /*cur_file.high_size*/;

				ed_write(p1,p);

				cur_file.high_size-=p;

				p1=cur_file.buffer+cur_file.buf_size;
				p2=p1-p;

				l=cur_file.high_size;
				while (l--)
				{
					*--p1=(*--p2);
				}
			}
		}
		else
		{
			p=(-p);

#ifdef _DEBUG
			printf("dumping %ld from bottom\n",p);
#endif

			ed_reserve(p);

#ifdef _DEBUG
			if ((cur_file.t_lol+p)>cur_file.t_hip)
			{
#if defined(_WIN32) && defined(_M_IX86)
				__asm int 3;
#endif
			}
#endif

			/* move buffer to bottom half file */

			if (p > cur_file.low_size)
			{
				p=cur_file.low_size;
			}

			if (p)
			{
				ed_seek(cur_file.t_lol);
				ed_write(cur_file.buffer,p);

				p1=cur_file.buffer;
				p2=cur_file.buffer+p;

				cur_file.low_size-=p;
				cur_file.t_lol+=p;

				l=cur_file.low_size;

				while (l--)
				{
					*p1++=(*p2++);
				}
			}
		}
	}
	else
	{
		/* flush entire buffer */

		p=cur_file.low_size + cur_file.high_size;

#ifdef _DEBUG
		printf("dumping entire %ld buffer\n",p);
#endif

		if (ed_reserve(p))
		{
#if defined(_WIN32) && defined(_M_IX86)
			__asm int 3;
#endif
		}

		if (cur_file.low_size)
		{
			ed_dump(-cur_file.low_size);
		}

		if (cur_file.high_size)
		{
			ed_dump(cur_file.high_size);
		}
	}
}

static long ed_pos(void)
{
	return cur_file.low_size+cur_file.t_lol;
}

static void ed_seof(long l)
{
	while (forever)
	{
		long m;
		char buf[512];

		m=sizeof(buf);

		while (m--) buf[m]=0;

#ifdef TEMP_FP
		fseek(cur_file.t_fp,0L,SEEK_END);
		m=ftell(cur_file.t_fp);
#else
		m=lseek(cur_file.t_fd,0L,SEEK_END);
#endif

		m=l-m;

		if (m <= 0) 
		{
			if (m < 0)
			{
				perror("seof");
				tty_reset(0);
				exit(1);
			}

#ifdef TEMP_FP
			fflush(cur_file.t_fp);
#endif
			break;
		}

		if (m > sizeof(buf))
		{
			m=sizeof(buf);
		}

		ed_write(buf,m);
	}
}

static int ed_reserve(long l)
{
	ed_tzap();

	if (!l) return 0;

	l-=(cur_file.t_hip - cur_file.t_lol);

	if (l <= 0 )
	{
		/* enough space in file */

		return 0;
	}

#ifdef _DEBUG
	printf("ed_reserve %ld bytes\n",l);
#endif

	ed_seof(l+cur_file.t_hip+cur_file.t_hil);

	if (cur_file.t_hil)
	{
		/* have to shuffle up the high portion */

		long n=cur_file.t_hil;
		long src=cur_file.t_hil+cur_file.t_hip;
		long tgt=src+l;

#ifdef _DEBUG
		printf("shuffle(n=%ld,l=%ld)\n",n,l);
#endif

		while (n)
		{
			long m=n;
			char buf[512];

			if (m > sizeof(buf))
			{
				m=sizeof(buf);
			}

			src-=m;
			tgt-=m;
			n-=m;

			ed_seek(src);
			ed_read(buf,m);
			ed_seek(tgt);
			ed_write(buf,m);
		}
	}

	cur_file.t_hip+=l;

	return 0;
}

static void co_puts(char *p)
{
	while (*p) putchar(*p++);
}

static int tty_sz(int fd)
{
#ifdef TIOCGWINSZ
	struct winsize ws;
	ws.ws_row=ws.ws_col=0;
	if (!ioctl(fd,TIOCGWINSZ,&ws))
	{
		if ((ws.ws_col > 10) & (ws.ws_row > 4))
		{
			total_cols=ws.ws_col;
			total_lines=ws.ws_row;
			if (total_lines > max_lines) total_lines=max_lines;
			return 0;
		}
	}
#endif /* TIOCGWINSZ */

#ifdef _WIN32
	{
		int cols,rows;
		if (!tty_winsize(&cols,&rows))
		{
			total_cols=cols;
			total_lines=rows;
			if (total_lines > max_lines) total_lines=max_lines;
			return 0;
		}
	}
#endif

#ifdef ANSI_SYS
#	ifdef PLOT_CTRL
	/* plot cursor at bottom right and ask for the position */
/*	co_puts("\033[127;127H\033[6n");*/
	guessing=1;
	co_puts("\033[127;127H\033[6n");
	fflush(stdout);
	if (ctrl_w==getkey())
	{
		return 0;
	}
#	endif /* PLOT_CTRL */
#else /* ANSI_SYS */
#	ifdef _WIN32
	co_puts("\033[?2l");
	total_lines=25;
#	endif /* _WIN32 */
	guessing=1;
	co_puts("\033Z");
	fflush(stdout);
	if (ctrl_w==getkey())
	{
		return 0;
	}
#endif /* ANSI_SYS */

	return -1;
}

static int getkey(void)
{
	int k;

#ifdef _DEBUG
	dumpinfo();
#endif

	if (winched)
	{
		int cols=total_cols;
		int rows=total_lines;
		winched=0;

		if (!tty_sz(0))
		{
			if ((cols!=total_cols)||(rows!=total_lines))
			{
				return ctrl_w;
			}
		}
	}

	switch (k=co_getch()) 
	{
	case escape:
		switch (k=co_getch()) 
		{
		case 'A':	
			k=key_up; 
			break;
		case 'B':	
			k=key_down; 
			break;
		case 'C':	
			k=key_right; 
			break;
		case 'D':	
			k=key_left; 
			break;
		case 'P':	
			k=escape; 
			break;
		case '/':
			k=co_getch();
			k=ctrl_w;
			guessing=0;	/* VT52 response, last should have been 'Z' */
			break;
		case 'O':
		case '[':
			{
				int index=10;
				int args[10];

				while (index)
				{
					index--;
					args[index]=0;
				}

				do
				{
					k=co_getch();

					switch (k) 
					{
						case 'A':	
							k=key_up; 
							break;
						case 'B':	
							k=key_down; 
							break;
						case 'C':	
							k=key_right; 
							break;
						case 'D':	
							k=key_left; 
							break;
						case 'P':	
							k=escape; 
							break;
						case '~':
							switch (args[0])
							{
							case 1:
								return key_find;
							case 2:
								return key_ins;
							case 3:
								return key_del;
							case 4:
								return key_sel;
							case 5:
								return page_up;
							case 6:
								return page_down;
							default:
								k=escape;
								break;
							}
							break;
						case ';':
							index++;
							k=0;
							break;
						case 'R':
							total_lines=args[0];
							total_cols=args[1];
							k=ctrl_w;
							guessing=0;
							break;
						case 'c':
							k=ctrl_w;
							break;
						default:
							if ((k >= '0') & (k <= '9'))
							{
								args[index]*=10;
								args[index]+=(k-'0');
							}

							k=0;
							break;
					}

				} while (k==0);
			}
		}
		break;
	case ctrl_s:	
		k=key_left; 
		break;
	case ctrl_d:	
		k=key_right; 
		break;
	case ctrl_x:	
		k=key_down; 
		break;
	case ctrl_e:	
		k=key_up; 
		break;
	case ctrl_q:
		k=key_home;
		break;
	case ctrl_p:
	case ctrl_u:
	case ctrl_r:
		k=page_up; 
		break;
	case ctrl_c:
	case ctrl_n:	
		k=page_down; 
		break;
#ifdef INTELLEC
	case 0x1e:	
		k=key_up; 
		break;
	case 0x1c:	
		k=key_down; 
		break;
	case 0x14:	
		k=key_right; 
		break;
	case 0x1f:	
		k=key_left; 
		break;
	case 0x1d:	
		k=key_home; 
		break;
#else
#ifndef QNX
	case 0x1e:	
		k=key_up; 
		break;
#endif
	case 0x1f:	
		k=key_down; 
		break;
	case 0x1c:	
		k=key_left; 
		break;
	case 0x1d:	
		k=key_right; 
		break;
#endif
	case 0xa3:	
		k&=0x7f; 
		break;
	}

	if (!k) 
	{
		switch (k=co_getch()) 
		{
		case 0x3b: 
			k=escape; 
			break;
		case 0x47: 
			k=key_start; 
			break;
		case 0x48: 
			k=key_up; 
			break;
		case 0x49: 
			k=page_up; 
			break;
		case 0x4B: 
			k=key_left; 
			break;
		case 0x4C: 
			k=key_home; 
			break;
		case 0x4D: 
			k=key_right; 
			break;
		case 0x4F: 
			k=key_end; 
			break;
		case 0x50: 
			k=key_down; 
			break;
		case 0x51: 
			k=page_down; 
			break;
		case 0x52: 
			k=key_ins; 
			break;
		case 0x53: 
			k=key_del; 
			break;
		}
	}

	if (k==key_home) 
	{
		switch (k=getkey()) 
		{
		case key_up: 
			k=page_up; 
			break;
		case key_down: 
			k=page_down; 
			break;
		case key_left: 
			k=key_start; 
			break;
		case key_right: 
			k=key_end; 
			break;
		case key_home: 
			k=escape; 
			break;
		}
	}

	return k;
}

static void co(int c)
{
	int i;
	switch (c) 
	{
	case nl:
		putchar(13);
		putchar(c);
		tty_col=0;
		tty_row++;
		break;
	default:
#ifndef REVERSE
		if (rev_on==2) c='#';
#endif
		if (c<32) c='?'; 
		else if (c>126) c='?';
		putchar(c);
		tty_col++;
		if ((c!=' ') | rev_on) 
		{
			if (tty_col > chars_per_line[tty_row]) 
			{
				chars_per_line[tty_row]=tty_col;
			}
		}
		if (tty_col > last_col) 
		{ 
			if (wordwrap) 
			{
				tty_col=0; 
				tty_row++;
			} 
			else 
			{
				tty_col=last_col;
			}
		}
		break;
	}
	if (tty_row > menu_line) 
	{
		tty_row=menu_line;
		i=0;
		while (i < menu_line) 
		{
			chars_per_line[i]=chars_per_line[i+1];
			i++;
		}
		chars_per_line[menu_line]=0;
		menu_erased=1;
	}
}

static void co_str(char *p)
{
	while (*p) co(*p++);
}

static int next_tab(int i)
{
	i/=tabs;
	i++;
	i*=tabs;
	return i;
}

static int next_col(int c,int ch)
{
	if (ch==nl) return 0;
	if (ch==tab) return next_tab(c);
	if (c>=last_col) 
	{
		return 0;
	}
	return (c+1);
}

static char *sprintn(long u,char *p)
{
	long v;

	v=u/10;
	if (v) p=sprintn(v,p);
	v=u%10;
	*p++=(char)('0'+v);
	*p=0;
	return p;
}

static void printn(long u)
{
	char buf[10];
	sprintn(u,buf);
	co_str(buf);
}

static void reverse(int i)
{
	if (!(rev_on | i)) return;

	if (i==4) return;

	rev_on=i;
#ifdef REVERSE
#ifdef ANSI_SYS
	if (i) 
	{
		co_puts("\033[7m");
	} 
	else 
		{
		co_puts("\033[0m");						 /* was 0 */
	}
#else /* ANSI_SYS */
	if (i) 
	{
		co_puts("\033(");
	} 
	else 
		{
		co_puts("\033)");
	}
#endif /* ANSI_SYS */
#else /* REVERSE */
	if (rev_on==3) 
	{
		if ((mode=='b')|(mode=='d')) rev_on=2;
	}
#endif /* REVERSE */
}

static void clear_line(void)
{
	int rev,i,j,k;

	rev=rev_on;
	if (rev) reverse(0);

	j=i=tty_col;
	k=tty_row;

	if (chars_per_line[k] > i) i=chars_per_line[k];
	if (k>=menu_line) 
	{
		if (i>last_col) i=last_col;
		if (j>last_col) j=last_col;
	}
	i-=j;
#ifdef ERASE_LINE
	if (i) 
	{
#ifdef ANSI_SYS
		co_puts("\033[K");
#else /* ANSI_SYS */
		co_puts("\033K");
#endif /* ANSI_SYS */
	}
#else /* ERASE_LINE */
	while (i--) co(' ');
#endif /* ERASE_LINE */
	
	chars_per_line[k]=j;

	if (rev) reverse(rev);
}

#ifdef PLOT_CTRL
static void plot_ctrl(int r,int c)
{
	char plot_buf[20];
#ifdef ANSI_SYS
	char num_buf[10];

	strcpy(plot_buf,"\033[");
	sprintn((long)(r+1),num_buf);
	strcat(plot_buf,num_buf);
	strcat(plot_buf,";");
	sprintn((long)(c+1),num_buf);
	strcat(plot_buf,num_buf);
	strcat(plot_buf,"H");
#else /* ANSI_SYS */
	char *p;
	p=plot_buf;
	*p++=(char)0x1b;
	*p++=(char)'Y';
	*p++=(char)32+r;
	*p++=(char)32+c;
	*p=0;
#endif /* ANSI_SYS */
	co_puts(plot_buf);
	tty_row=r; 
	tty_col=c;
}
#endif

#ifndef PLOT_CTRL
int mag(i)
int i;
{
	if (i<0) return -i;
	return i;
}
#endif /* PLOT_CTRL */

#ifdef SCROLL_RGN
static void scrol_rgn(int r,int c)
{
	char scr_buf[20];
	char num_buf[10];

	strcpy(scr_buf,"\033[");
	if (r != -1)
	{
		sprintn((long)(r+1),num_buf);
		strcat(scr_buf,num_buf);
	}
	strcat(scr_buf,";");
	if (c != -1)
	{
		sprintn((long)(c+1),num_buf);
		strcat(scr_buf,num_buf);
	}
	strcat(scr_buf,"r");
	co_puts(scr_buf);
	tty_row=(-1); 
	tty_col=(-1);
	plot_ctrl(0,0);
}
#endif

static void plot(int r,int c)
{
	if ((tty_row==r)&(tty_col==c)) return;

#ifndef PLOT_CTRL
	{
		int x,y;

		if ((!c) & (r>tty_row)) co(nl);

		x=mag(tty_col-c);
		y=mag(tty_row-r);

		if ((x+y) > (c+r)) home();

		while (r > tty_row) cud();
		while (r < tty_row) cuu();
		while (c > tty_col) cuf();
		while (c < tty_col) cub();
	}
#else /* PLOT_CTRL */
	plot_ctrl(r,c);
#endif /* PLOT_CTRL */
}

static void show_block(long q,long i,int endrow)
{
	if (row >= endrow) return;

	while (i--) 
	{
		int c;

		c=ed_at(q++);

		if (c==nl) 
		{
			clear_line();
			row++;
			plot(row,0);
			col=0;
			if (row>=endrow) return;
		} 
		else 
			{
			if (c==tab) 
			{
				int j;
				j=next_tab(col)-col;
				while (j--) 
				{
					co(' ');
					col++;
				}
			} 
			else 
			{
				co(c);
				col++;
			}
			if (col > last_col) 
			{ 
				col=0;
				row++;
#ifdef PLOT_CTRL
				plot(row,col);
#else
				if (!wordwrap) plot(row,col);
#endif
				if (row>=endrow) return;
			}
		}
	}
}

static void show_text(long pos,long len,int endrow)
{
	long inv_start,inv_end,k,l;

	inv_start=inv_end=0;
	if (sel_on) 
	{
		inv_start=sel_pos;
		inv_end=ed_pos();
		if (inv_start > inv_end) 
		{
			inv_start=ed_pos();
			inv_end=sel_pos;
		}
	}
	k=pos+len;
	if (inv_start < pos) inv_start=pos;
	if (inv_start > k) inv_start=k;
	if (inv_end < pos) inv_end=pos;
	if (inv_end > k) inv_end=k;

	l=inv_start-pos;
	if (l) show_block(pos,l,endrow);
	l=inv_end-inv_start;
	if (l) 
	{
		reverse(3);
		show_block(inv_start,l,endrow);
		reverse(0);
	}
	l=k-inv_end;
	if (l) show_block(inv_end,l,endrow);
}

static void show_top(void)
{
	row=col=0;
	plot(row,col);
	show_text(page_start,ed_pos()-page_start,status_line);
	crsr_row=row;
	crsr_col=col;
}

static void show_bottom(int erase_after)
{
	plot(row=crsr_row,col=crsr_col);
	show_text(ed_pos(),ed_len()-ed_pos(),status_line);
	/*	if (row < status_line) co('|');*/
	if (erase_after) 
	{
		if (row < status_line) clear_line();

		row++;

		while (row < status_line) 
		{
			if (chars_per_line[row])
			{
				plot(row,0);
				clear_line();
			}
			/*		co_str("\n");*/
			row++;
		}
	}
	if (erase_after != 2) plot(row=crsr_row,col=crsr_col);
	fflush(stdout);
}

static void scan_pos(long i)
{
	long p=page_start;
	row=col=0;
	while (i--) 
	{
		int c;
		if (p >= ed_len()) 
		{
			return;
		}

		c=ed_at(p++);

		col=next_col(col,c);

		if (col==0)
		{
			row++;

			if (row >= status_line)
			{
				return;
			}
		}
	}
}

static void paint_text(long len)
{
	long i;

	if (len==0) return;

	if (len < 0) 
	{
		i=ed_pos()+len;
		if (i < page_start) i=page_start;
		i-=page_start;
		scan_pos(i);
		plot(row,col);
		show_text(page_start+i,ed_pos()-page_start-i,status_line);
	} 
	else 
	{
		scan_pos(ed_pos()-page_start);
		plot(row,col);
		show_text(ed_pos(),len,status_line);
	}
}

static void ed_fmove(long i)
{
#ifdef _DEBUG
	if (cur_file.low_size+cur_file.high_size)
	{
#if defined(_WIN32) && defined(_M_IX86)
		__asm int 3
#endif
	}
	printf("ed_fmove(%ld)\n",i);
#endif

	if (i)
	{
		if (i > 0)
		{
			/* move higher to lower */

#ifdef _DEBUG
			if (i > cur_file.t_hil)
			{
#if defined(_WIN32) && defined(_M_IX86)
				__asm int 3;
#endif
			}
#endif
			while (i)
			{
				long l=i;

				if (l > cur_file.buf_size) 
				{
					l=cur_file.buf_size;
				}

				ed_seek(cur_file.t_hip);
				ed_read(cur_file.buffer,l);

				cur_file.t_hip+=l;
				cur_file.t_hil-=l;

				ed_seek(cur_file.t_lol);
				ed_write(cur_file.buffer,l);

				cur_file.t_lol+=l;

				i-=l;
			}
		}
		else
		{
			i=(-i);

#ifdef _DEBUG
			if (i > cur_file.t_lol)
			{
#if defined(_WIN32) && defined(_M_IX86)
				__asm int 3;
#endif
			}
#endif

			/* move lower to higher */

			while (i)
			{
				long l=i;

				if (l > cur_file.buf_size)
				{
					l=cur_file.buf_size;
				}

				ed_seek(cur_file.t_lol-l);
				ed_read(cur_file.buffer,l);

				cur_file.t_lol-=l;

				ed_seek(cur_file.t_hip-l);
				ed_write(cur_file.buffer,l);

				cur_file.t_hip-=l;
				cur_file.t_hil+=l;
				i-=l;
			}
		}
	}
}

static void ed_move(long i)
{
	/* if +ve add text to lower section, else move to upper half */

	ADBG_POS(ed_pos());
	ADBG_LEN(ed_len());

	ADBG_MOVE(i)

	if (i) 
	{
		char *p,*q;

		p=cur_file.buffer;
		p+=cur_file.low_size;
		q=cur_file.buffer;
		q+=(cur_file.buf_size-cur_file.high_size);

		if (i < 0) 
		{
			i=(-i);

			if (i > cur_file.low_size)
			{
				/* flush memory buffer totally */
				ed_dump(0L);

				ed_fmove(-i);

				i=ed_room();
				i>>=1;
				ed_load(i);
				ed_load(-i);
			}
			else
			{
				cur_file.high_size+=i;
				cur_file.low_size-=i;

#ifdef _WIN32
				if (cur_file.high_size < 0)
				{
#if defined(_WIN32) && defined(_M_IX86)
					__asm int 3
#endif
				}
				if (cur_file.low_size < 0)
				{
#if defined(_WIN32) && defined(_M_IX86)
					__asm int 3;
#endif
				}
#endif

				while (i--) *--q=(*--p);
			}
		} 
		else 
		{
			if (i > cur_file.high_size)
			{
				ed_dump(0L);
				ed_fmove(i);

				i=ed_room();
				i>>=1;
				ed_load(i);
				ed_load(-i);
			}
			else
			{
				cur_file.high_size-=i;
				cur_file.low_size+=i;

#ifdef _WIN32
				if (cur_file.high_size < 0)
				{
#if defined(_WIN32) && defined(_M_IX86)
					__asm int 3
#endif
				}
				if (cur_file.low_size < 0)
				{
#if defined(_WIN32) && defined(_M_IX86)
					__asm int 3
#endif
				}
#endif

				while (i--) *p++=(*q++);
			}
		}
	}
}

static void show_mopt(char **list,int opt)
{
int i=0;
int pm=0;

	plot(menu_line,0);

	while (list[i])
	{
		i++;
	}

	if (opt >= i)
	{
		opt=0;
	}

	while (opt < i)
	{
		char *p=list[opt];
		size_t k=strlen(p);

		if ((tty_col+10+9) > total_cols)
		{
			break;
		}

		opt++;

		co_str("  ");
		co_str(p);
		k=10-k-2;
		while (k--) co(' ');
		pm++;
	}

	while (tty_col < (last_col-9))
	{
		co(' ');
	}

	if (pm < i)
	{
		menu_nxt=opt;
		co_str("--more--");
	}
	else
	{
		menu_nxt=0;
	}

	while (tty_col < last_col)
	{
		co(' ');
	}
}

char *menu_d[]={"Again","Block","Delete","Find","-find","Get","Insert","Jump","Length","Quit","Replace","?replace","Shell","View","Xchange",NULL};
char *menu_b[]={"Buffer","Copy","Delete","Put",NULL};
char *menu_q[]={"Abort","Exit","Init","Update","Write",NULL};

static void show_status(void)
{
#if defined(_WIN32) && defined(_M_IX86)
/*	if (!menu_erased)
	{
		__asm int 3;
	}*/
#endif

	plot(status_line,0);

	menu_erased=0;

	switch (mode) 
	{
	case 'i':
		break;
	default:
		co_str(" ---- ");
		if (filename) 
		{
			co_str(filename);
		} 
/*		else 
		{
			co_str("(untitled)");
		}*/

		if (show_size) 
		{
			long i=ed_len();
	/*		if (i==cur_file.buf_size) 
			{
				co(' ');
				reverse(1);
				co_str("full");
				reverse(0);
			}*/
			if (filename) co_str(", ");
			if (show_size==2) 
			{
				co_str("saving...");
			} 
			else 
			{
				printn(i);
				co_str(" bytes");
	/*			printn(cur_file.buf_size-i);
				co_str(" free");*/
			}
			show_size=0;
		}
	}
	clear_line();
	plot(menu_line,0);
	switch (mode) 
	{
	case 'i':
		co_str("[insert]");
		break;
	case 'x':
		co_str("[exchange]");
		break;
	case 'b':
	case 'd':
		reverse(rev_menu);
		show_mopt(menu_b,0);
		reverse(0);
		break;
	case 'q':
		reverse(rev_menu);
		show_mopt(menu_q,0);
		reverse(0);
		break;
	default:
		reverse(rev_menu);
		show_mopt(menu_d,menu_id);
		reverse(0);
	}
	clear_line();
}

static void cls(void)
{
	int i;
#ifdef ANSI_SYS
	co_puts("\033[H\033[2J");
#else
	co_puts("\033H\033J");
#endif
	tty_row=tty_col=i=0;
	while (i < total_lines) chars_per_line[i++]=0;
	menu_erased=1;
}

static int init(char *fname)
{
	FILE *fptr;
	int c;

	c=0;
	while (c < total_lines) chars_per_line[c++]=total_cols;

	if (filename)
	{
		free(filename);
		filename=NULL; 
	}

	crsr_row=crsr_col=0;
	cur_file.low_size=page_start=0L;
	cur_file.high_size=0;

	cur_file.t_hip=0;
	cur_file.t_hil=0;
	cur_file.t_lol=0;
	cur_file.t_calen=0;
	cur_file.t_capos=0;

	if (fname)
	{
		filename=strdup(fname);
	}

#ifdef CLEAR_SCREEN
	cls();
#else
#ifndef ERASE_LINE
	{
		int i;
		i=max_lines << 1;
		while (i--) co(nl);
	}
#else
	home();
#endif
#endif
	show_status();
	plot(0,0);
	fflush(stdout);

	if (!filename) 
	{
		show_bottom(1);
		fflush(stdout);
		return 0;
	}

	fptr=fopen(filename,"r");

	if (!fptr) 
	{
		show_bottom(1);
		fflush(stdout);
		return 0;
	}

/*	while (EOF!=(c=getc(fptr))) 
	{
		ed_ins(c);
	}
*/
	ed_seof(cur_file.t_hip);
	ed_seek(cur_file.t_hip);

	while (forever)
	{
		int i=(int)fread(cur_file.t_cache,1,sizeof(cur_file.t_cache),fptr);
		if (i > 0)
		{
			long l=i;
			ed_write(cur_file.t_cache,l);

			ADBG_MINS(cur_file.t_cache,l)

			cur_file.t_hil+=l;
		}
		else
		{
			break;
		}
	}

	ed_tzap();

	fclose(fptr);
	ed_move(-ed_pos());
	ed_load(cur_file.buf_size);
	show_size=1;
	show_status();
	show_top();
	show_bottom(1);
	fflush(stdout);
	return 0;
}      

static void plot_cursor(void)
{
	plot(crsr_row,crsr_col);
}

static void write_file(char *fn)
{
#ifdef _DEBUG
	long i=ed_len();
	long p=0;
	while (i--)
	{
		ed_at(p);
		p++;
	}
#else
	FILE *f;

	if (!fn) return;
	if (!fn[0]) return;

	f=fopen(fn,"w");
	if (f) 
	{
		long i=ed_len();
		long p=0;
		while (i--)
		{
			int c=ed_at(p);
			putc(c,f);
			p++;
		}
		fclose(f);
	}
#endif
}


#ifndef PLOT_CTRL

cuu()
{
	tty_row--;
#ifdef ANSI_SYS
	co_puts("\033[A");
#else
	co_puts("\033A");
#endif
}

cud()
{
	tty_row++;
#ifdef ANSI_SYS
	co_puts("\033[B");
#else
	co_puts("\033B");
#endif
}

cuf()
{
	tty_col++;
#ifdef ANSI_SYS
	co_puts("\033[C");
#else
	co_puts("\033C");
#endif
}

cub()
{
	tty_col--;
#ifdef ANSI_SYS
	co_puts("\033[D");
#else
	co_puts("\033D");
#endif
}

home()
{
	tty_row=tty_col=0;
#ifdef ANSI_SYS
	co_puts("\033[H");
#else
	co_puts("\033H");
#endif
}
#endif


static void move_right(void)
{
	if (ed_pos()!=ed_len()) 
	{
		ed_move(1L);
		if (sel_on) paint_text(-1L);
	}
}

static void move_left(void)
{
	if (ed_pos()) 
	{
		ed_move(-1L);
		if (sel_on) paint_text(1L);
	}
}

static void show_line(void)
{
	long p=ed_pos();
	long i,j;
	j=0;
	i=ed_len()-p;
	while (i--) 
	{
		int c=ed_at(p++);
		if (c==nl) 
		{
			i=0;
		} 
		else 
		{
			j++;
		}
	}

	if (j) show_text(ed_pos(),j,status_line);
	clear_line();
}

/* upto CR but not including it */

static long line_len(long p)
{
	long i=0;
	long j=ed_len()-p;

	while (j--)
	{
		if (ed_at(p++)==nl)
		{
			break;
		}

		i++;
	}

	return i;
}

static void delete_char(void)
{
	int new_line=0;
	long pos=ed_pos();

	if (!pos) return;

	row=crsr_row;
	col=crsr_col;

	if (!col) 
	{
		new_line=1;
	} 
	else 
	{
		long p=ed_pos()-1;
		long i=line_len(p);

		while (i--) 
		{
			int c=ed_at(p++);
			col=next_col(col,c);
			if (!col)
			{
				new_line=1;
				i=0;
			}
		}
	}

	/* could add a special case, -
		if we are deleting character at <0,0>
		then paint the previous line
		at the current line */

	ed_del(-1L);

	find_cursor();
	plot_cursor();
	row=crsr_row; 
	col=crsr_col;
	if (new_line) 
	{
		/*	clear_end();*/
		show_bottom(1);
		/*	show_status();*/
	} 
	else 
	{
		show_line();
		plot_cursor();
	}
}

static long sl_phys(long p)
{
	/* this returns the start of this line by going back to a previous CR */
	while (p)
	{
		p--;

		while (ed_at(p)==nl)
		{
			p++;

			return p;
		}
	}

	return p;
}

static long sl_prev(long p)
{
	/* previous actual line start */

	p=sl_phys(p);

	if (p)
	{
		p--;

		p=sl_phys(p);
	}

	return p;
}

static long next_log(long p)
{
	/* find next line either by start of newline, or wrapped the columns */
	int c=0;
	long i=ed_len()-p;	/* see how many characters left */

	while (i--)
	{
		int ch=ed_at(p++);

		c=next_col(c,ch);

		if (!c)
		{
			break;
		}
	}

	return p;
}

static long prev_log(long p)
{
	/* previous line as seen on the VDU */
	long l=sl_prev(p);

	while (l < p)
	{
		long m=next_log(l);
		if (m < p)
		{
			l=m;
		}
		else
		{
			break;
		}
	}

	return l;
}

static long sl_this(long p)
{
	long q=sl_phys(p);
	int col=0;
	long i=ed_len()-q;
	long m=q;

	while (i--)
	{
		if (q >= p)
		{
			break;
		}
		else
		{
			int c=ed_at(q++);

			col=next_col(col,c);

			if (col==0)
			{
				m=q;
			}
		}
	}

	return m;
}

static int col_at(long p)
{
	long q=sl_this(p);
	int col=0;
	long i=p-q;

	while (i--)
	{
		char c=(char)ed_at(q++);

		col=next_col(col,c);
	}

	return col;
}

#ifdef NEED_ROW_AT
static int row_at(long p)
{
	int row=0;
	int col=0;
	long q=page_start;
	long i=p-q;
	if (p < page_start) return -1;

	while (i--)
	{
		int c=ed_at(q++);

		col=next_col(col,c);

		if (!col)
		{
			row++;
		}

		if (row > (last_text+1))
		{
			i=0;
		}
	}

	return row;
}
#endif

void repage(void)
{
	int r=5;
	page_start=ed_pos();

	while (r) 
	{
		page_start=prev_log(page_start);

		r--;

		if (!page_start) r=0;
	}

	/*	if (menu_erased) show_status();*/
	show_top();
	show_bottom(2);
	if (menu_erased) show_status();
	plot_cursor();
}

#ifdef SCROLL_RGN
static void scrol_up(void)
{
	int i;
	scrol_rgn(0,last_text);
	plot_ctrl(last_text,0);
	co_puts("\033E"); /* next line */
	scrol_rgn(0,menu_line);
	plot_ctrl(last_text,0);
	i=0;
	while (i < last_text)
	{
		chars_per_line[i]=chars_per_line[i+1];
		i++;
	}
	chars_per_line[i]=0;

}
static void scrol_down(void)
{
	int i;
	scrol_rgn(0,last_text);
	plot_ctrl(0,0);
	co_puts("\033M");
	scrol_rgn(0,menu_line);
	plot_ctrl(0,0);
	i=last_text;
	while (i--)
	{
		chars_per_line[i+1]=chars_per_line[i];
	}
	chars_per_line[i]=0;
}
#endif

static void find_cursor(void)
{
	if (page_start > ed_pos()) 
	{
#ifdef SCROLL_RGN
		if (page_start > ed_len())
		{
			repage();
		}
		else
		{
			long prev=prev_log(page_start);
			if (prev_log(page_start) <= ed_pos())
			{
				long i=0;
				scrol_down();
				plot(0,0);
				row=0;
				col=0;
				i=page_start-prev;	/* howmany to paint */
				page_start=prev;
				show_text(page_start,page_start+i,1);
			}
			else
			{
				repage();
			}
		}
#else
		repage();
#endif
	}

	while (forever)
	{
		long p=page_start;
		long i=ed_pos()-page_start;

		row=col=0;

		while (i--) 
		{
			int c=ed_at(p++);

			col=next_col(col,c);

			if (!col)
			{
				row++;

				if (row > status_line)
				{
					break;
				}
			}

		}

		if (row<status_line) 
		{
			crsr_row=row; 
			crsr_col=col;

			break;
		} 
		else 
		{
			if (row==status_line) 
			{
#ifdef SCROLL_RGN
				scrol_up();
#else
				plot(status_line,0);
				clear_line();
				plot(menu_line,0);
				clear_line();
#ifdef PLOT_CTRL
				plot(menu_line,0);								 /* this will get on the last line */
#endif
				co(nl);
#endif
				crsr_row=row=last_text;
				crsr_col=col;

				p=page_start;
				i=ed_pos()-page_start;
				col=0;
				while (i--) 
				{
					int c=ed_at(p++);
					page_start++;
					col=next_col(col,c);
					if (!col) 
					{
						i=0;
					}
				}

				{
					long l=ed_pos();
					long m=sl_this(l);

					paint_text(m-l);
				}
				show_bottom(0);
				/*		show_status();*/

				break;
			} 
			else 
			{
				repage();
			}
		}
	}
}

static void move_up(void)
{
	long i;
	long p;
	int c2=0;

/*	if (!crsr_row) 
	{
		if (!page_start) return;
		repage();
		if (!crsr_row) return;
	}
*/
/*	crsr_row--;*/

	p=ed_pos();
	i=sl_this(p);
	i=prev_log(i);
	col=col_at(p);

	while (i < p)
	{
		c2=next_col(c2,ed_at(i));
		if (!c2) 
		{
			break;
		}
		if (c2 > col)
		{
			break;
		}
		i++;
	}

	i=i-p;

	ed_move(i);
	if (sel_on) paint_text(-i);
}

static void move_end(void)
{
	long p=ed_pos();
	long i=ed_len()-p;
	long j=0;

	while (i--) 
	{
		if (ed_at(p++)==nl) i=0; 
		else j++;
	}

	ed_move(j);

	if (sel_on) paint_text(-j);
}

static void move_home(void)
{
	long j=sl_this(ed_pos());
	long k=ed_pos()-j;

	ed_move(-k);
	if (sel_on) paint_text(k);
}

static void move_down(void)
{
	long p=ed_pos();
	long i=ed_len()-p;
	long j=0;

	row=crsr_row; 
	col=crsr_col;
	crsr_row++;

	while (i--) 
	{
		int c=ed_at(p++);

		col=next_col(col,c);

		if (!col)
		{
			row++;
		}

		if (crsr_row < row) 
		{
			break;
		}

		j++;

		if ((crsr_row==row) & (col>=crsr_col)) 
		{
			break;
		}
	}

	ed_move(j);
	if (sel_on) paint_text(-j);
}

static void flip_up(void)
{
	long pos=ed_pos();
	long l=sl_this(pos);
	int k=total_lines-5;
	long d;

	while (k--)
	{
		if (!l) break;

		l=prev_log(l);
	}

	d=l-pos;
	ed_move(d);
	if (sel_on) paint_text(-d);
}

static void flip_down(void)
{
	long p=ed_pos();
	int j=total_lines-5;
	long i=ed_len()-p;
	int col=0;

	p=sl_this(p);

	while (i--) 
	{
		int ch=ed_at(p++);
		col=next_col(col,ch);
		if (!col)
		{
			j--;
		}
		if (j < 0)
		{
			i=0;
		}
	}

	p-=ed_pos();

	ed_move(p);
	if (sel_on) paint_text(-p);
}

static int cur_key(int i)
{
	switch (i) 
	{
	case ctrl_w:
		ed_tzap();
		cls(); 
		menu_erased=1;
		repage(); 
		break;
	case page_up:
		if (ed_pos())
		{
			flip_up(); 
		}
		break;
	case page_down: 
		flip_down(); 
		break;
	case 8:
	case key_del:
	case 0x7f: 
		delete_char(); 
		break;
	case key_left: 
		move_left(); 
		break;
	case key_right: 
		move_right(); 
		break;
	case key_up: 
		move_up(); 
		break;
	case key_down: 
		move_down(); 
		break;
	case key_start:
		move_home();
		break;
	case key_end:
		move_end();
		break;
	default: 
		return 0;
	}
	find_cursor();
	plot_cursor();
	fflush(stdout);
	return 1;   
}

static void insert_text(void)
{
	int c;
	menu_erased=1;
	show_status();
	plot_cursor();
	fflush(stdout);
	while (escape!=(c=getkey())) 
	{
		if (c==key_ins) break;
		if (c==ctrl_z				 /*|(c==key_home)*/ ) break;
		if (c==key_find) break;
		if (c==key_sel) break;

#ifdef SQUASH_CR
		if (c==13) c=-1;
#endif

		if (c==-1) 
		{
			continue;
		}

		if (!cur_key(c)) 
		{
			long p=ed_pos(),i;
			int new_line=0;

			if (c==13) c=nl;


			if (mode=='i') 
			{
				ed_ins(c);
				ed_move(-1L);
			} 
			else 
			{
				if (p != ed_len())
				{
					if (nl != ed_at(p))
					{
						ed_del(1L);
					}
				}

				ed_ins(c);
				ed_move(-1L);
			}

			if (c==nl) new_line=1;

			row=crsr_row; 
			col=crsr_col;
			i=ed_len()-p;

			while (i--) 
			{
				c=ed_at(p++);

				if (c==nl) break;

				col=next_col(col,c);

				if (!col)
				{
					i=0;
					new_line=1;
				}
			}

			row=crsr_row; 
			col=crsr_col;
			plot_cursor();
			if (new_line) 
			{
				/*	clear_end();*/
				show_bottom(2);
				/*	show_status();*/
			} 
			else 
			{
				long j=0;
				p=ed_pos();
				i=ed_len()-p;
				while (i--) 
				{
					if (ed_at(p++)==nl)
					{
						i=0;
					}
					else
					{
						j++;
					}
				}
				show_text(ed_pos(),j,1+crsr_row);
				clear_line();
			}
			ed_move(1L);
			find_cursor();
			plot_cursor();
			fflush(stdout);
		}     	
	}
}

static void save_select(char *file)
{
	FILE *fptr;
	unsigned int i;
	long p=ed_pos();

	if ((file==NULL) || !file[0]) return;

	if (sel_pos==ed_pos()) return;

	if (sel_pos > ed_pos()) 
	{
		i=sel_pos-p;
	} 
	else 
	{
		i=p-sel_pos;
		p=sel_pos;
	}

	fptr=fopen(file,"w");

	if (fptr) 
	{
		while (i--) 
		{
			putc(ed_at(p),fptr);
			p++;
		}
		fclose(fptr);
	}
}

static char * enter_fname(char *fstr)
{
	int i=0,c,j;

	menu_erased=1;

	plot(menu_line,0);
	co_str(fstr);
	c=tty_col;
	clear_line();
	plot(menu_line,c);
	fflush(stdout);

	while (nl!=(c=getkey())) 
	{
		if ((c==13)|(c==escape)) break;
		if (c > 0)
		{
			switch (c) 
			{
			case 8:
			case key_del:
			case 0x7f:
				if (i) 
				{
					j=tty_col-1;
					plot(menu_line,j);
					clear_line();
					i--;
				}
				break;
			default:
				get_fname[i++]=(char)c;
				co(c);
			}

			fflush(stdout);
		}
	}

	get_fname[i]=0;
	if (!i) 
	{
		co_str(clip_name);
		return clip_name;
	}
	return get_fname;
}

static void do_get(char *file)
{
	FILE *fptr;
	long p=ed_pos();
	long i=0;
	struct stat s;

	if ((file==NULL) || !file[0]) return;

	if (stat(file,&s))
	{
		return;
	}

	if (ed_reserve((long)s.st_size))
	{
		return;
	}

	fptr=fopen(file,"r");

	if (!fptr) return;

	while (forever) 
	{
		int c=getc(fptr);
		if (c==EOF) break;
		ed_ins(c);
	}

	fclose(fptr);

	sel_pos=ed_pos();
	i=sel_pos-p;

	ed_move(-i);
	sel_on=1;
	show_bottom(2);
	ed_move(i);
	sel_pos=ed_pos()-i;
	find_cursor();
	plot_cursor();
}

void do_clipboard(int c)
{
	switch(c) 
	{
	case 'b':
		save_select(clip_name);
		paint_text(sel_pos-ed_pos());
		break;
	case 'c':
		save_select(enter_fname("Copy text to - "));
		paint_text(sel_pos-ed_pos());
		break;
	case 'p':
		save_select(enter_fname("Put text in - "));
	case 'd':
		if (c=='d') save_select(clip_name);

		if (sel_pos==ed_pos()) return;
		if (sel_pos<ed_pos()) 
		{
			long d=ed_pos()-sel_pos;
			ed_del(-d);
			if (sel_pos < page_start) 
			{
				find_cursor();
				plot_cursor();
			} 
			else 
			{
				find_cursor();
				plot_cursor();
				/*	clear_end();*/
				show_bottom(1);
			}
		} 
		else 
		{
			long d=sel_pos-ed_pos();
			ed_del(d);
			/* plot_cursor();*/
			/*	clear_end();*/
			show_bottom(1);
		}
		break;
	default:
		paint_text(sel_pos-ed_pos());
		break;
	}
}

static long get_text_input(char *f_string)
{
	int i=0;
	int c,k;

	k=tty_col;

	co_str(f_string);
	co(0x22);
	clear_line();
	plot(menu_line,k);

	fflush(stdout);

	while (nl!=(c=getkey())) 
	{
		int j=0;
		if ((c==13)|(c==escape)|(c==ctrl_z)|(c==key_home))
			break;
		if (c==key_ins) break;

		if (c > 0)
		{
			switch(c) 
			{
			case key_del:
			case 8:
			case 0x7f:
				if (i) 
				{
					i--;
					j=tty_col-1;
				} else {
					j=k;
				}
				clear_line();
				plot(menu_line,j);
				break;
			default:
				if (!i) {
					clear_line();
					plot(menu_line,k);
					}
				f_string[i++]=(char)c;
				co(c);
			}
			f_string[i]=0;
			j=tty_col;
			co(0x22);
			plot(menu_line,j);
			fflush(stdout);
		}
	}
	i=0;
	while (f_string[i]) i++;
	plot(menu_line,k+i);
	menu_erased=1;

	return i;
}

static int match_string(long p,char *q,long i)
{
	while (i--) 
	{
		if (ed_at(p++)!=(*q++)) 
		{
			return 0;
		}
	}
	return 1;
}

void do_find(int m)
{
	char *q;
	long i,k,l,f;

	f=0;

	if (m=='a') 
	{
		char *p=f_string;
		i=0;
		while (*p++) i++;
	} 
	else 
	{
		last_cmd=m;

		plot(menu_line,0);

		switch (last_cmd) 
		{
		case '-':	
			co_str("-find \042");	
			break;
		case 'f':	
			co_str("Find \042");	
			break;
		case 'r':	
			co_str("Replace \042");	
			break;
		case '?':	
			co_str("?replace \042"); 
			break;
		}

		i=get_text_input(f_string);
	}

	if (i) 
	{
		if ((m=='r') | (m=='?'))
		{
			int j;
			co_str("\042 with \042");
			j=tty_col;
			co(0x22);
			clear_line();
			plot(menu_line,j);
			fflush(stdout);

			get_text_input(r_string);
		}

		if (last_cmd!='-') 
		{
			long pp=ed_pos();
			l=ed_len()-ed_pos();

			if (i <= l)
			{
				l-=i;
				k=0;
				while (l--) 
				{
					if (match_string(pp,f_string,i)) 
					{
						f++;
						l=0;
						switch (last_cmd) 
						{
						case 'f':
							ed_move(k+i);
							sel_on=1;
							sel_pos=ed_pos()-i;
							paint_text(sel_pos-ed_pos());
							break;
						case '?':
						case 'r':
							{
								ed_move(k);
								ed_del(i);
								sel_pos=ed_pos(); /* remember where deleted from */
								q=r_string;
								while (*q) 
								{
									ed_ins(*q++);
								}
								if (sel_pos != ed_pos())
								{
									sel_on=1;
								}
							/*	sel_pos=ed_pos()-i;*/
							/*	paint_text(sel_pos-ed_pos());*/
							/*	page_start=ed_pos()+1;*/
								repage();
							}
							break;
						}
						find_cursor();
					}
					pp++;
					k++;
				}
			}
		} 

		if (last_cmd=='-')
		{
			long l=ed_pos();
			if (i <= l)
			{
				l-=i;
				k=0;
				while (l--) 
				{
					if (match_string(l,f_string,i)) 
					{
						ed_move(l-ed_pos());
						sel_on=1;
						sel_pos=ed_pos()+i;
						paint_text(sel_pos-ed_pos());
						find_cursor();
						f++;
						l=0;
					}
					k--;
				}
			}
		}
	}
	if (menu_erased) show_status();
	plot_cursor();
	fflush(stdout);
	if (!f) again=0;
}

static void do_block(void)
{
	int c;
	mode='b';
	sel_on=1;
	sel_pos=ed_pos();
	menu_erased=1;
	show_status();
	plot_cursor();
	fflush(stdout);
	while (cur_key(c=getkey())) 
	{
		if (menu_erased) 
		{
			show_status();
			plot_cursor();
		}
	}
	sel_on=0;
	do_clipboard(c);
	mode=0;
	menu_erased=1;
	show_status();
	plot_cursor();
}

static int get_again(int k)
{
	int i=1;

	mode=0;
	again=k-'0';
	plot(status_line,0);
	co_str(" ---- ");
	co(k);
	clear_line();
	plot(status_line,7);

	while (forever) 
	{
		fflush(stdout);
		k=getkey();
		if ((k>='0')&(k<='9')) 
		{
			i++;
			co(k);
			again*=10;
			again+=k-'0';
		} 
		else 
		{
			switch (k) 
			{
			case 8:
			case key_del:
			case 0x7f:
				if (i) 
				{
					i--;
					plot(status_line,tty_col-1);
					co(' ');
					plot(status_line,tty_col-1);
					again/=10;
				}
				break;
			case 13:
				k=nl;	/* fall through */
			default:
				menu_erased=1;
				show_status();
				plot_cursor();
				return k;
			}
		}
	}

	return 0;
}

static void update(void)
{
	show_size=2;
	show_status();
	plot_cursor();
	write_file(filename);
	show_status();
	plot_cursor();
}

static void my_atexit(void)
{
/*	fflush(stdout);*/
	tty_reset(0);
}

static void do_jump(long l)
{
	long i=ed_len();
	long p=0;

	if (l) l--;	/* so that we're zero based */

	while (i--)
	{
		if (!l)
		{
			break;
		}

		if (ed_at(p++)==nl)
		{
			l--;
		}
	}

	p=p-ed_pos();

	ed_move(p);

	find_cursor();
	if (menu_erased)
	{
		show_status();
	}
	plot_cursor();
}

void do_shell(void)
{
	char *p=NULL;

	plot(menu_line,0);
	clear_line();
	plot(menu_line,0);
	menu_erased=1;
	fflush(stdout);

	my_atexit();

#ifdef _WIN32
	p=getenv("COMSPEC");
#endif /* _WIN32 */

	if (!p)
	{
		p=getenv("SHELL");
	}

	if (!p) p="/bin/sh";

	if (system(p))
	{
	}

	tty_raw(0);

	/* incase the screen changed during the shell */

	tty_sz(0);

	cur_key(ctrl_w);
}

static int ed_init(void)
{
#ifdef MALLOC_SIZE
	cur_file.buf_size=MALLOC_SIZE;
	cur_file.buffer=malloc((unsigned)cur_file.buf_size);
	if (!cur_file.buffer) 
	{
		fputs("Cant malloc\n",stderr);
		return 1;
	}
#endif
#ifdef TEMP_FP
	cur_file.t_fp=tmpfile();
/*	cur_file.fp=fopen("aedit.tmp","w+b");*/
	if (!cur_file.t_fp)
	{
		perror("tmpfile");
		return -1;
	}
#else
	{
		char buf[256];
		char *p=getenv("TMP");
		if (p)
		{
		}
		else
		{
			p="/tmp";
		}
		strcpy(buf,p);
		strcat(buf,"/aeXXXXXX");
		mktemp(buf);
#ifdef O_BINARY
		cur_file.t_fd=open(buf,O_CREAT|O_BINARY|O_RDWR|O_TRUNC,S_IREAD|S_IWRITE);
#else
		cur_file.t_fd=creat(buf,S_IREAD|S_IWRITE);
		if (cur_file.t_fd==-1)
		{
			perror(buf);
			return -1;
		}
		close(cur_file.t_fd);
#ifdef O_RDWR
		cur_file.t_fd=open(buf,O_RDWR);
#else
		cur_file.t_fd=open(buf,2);
#endif
#endif
		if (cur_file.t_fd==-1)
		{
			perror(buf);
			return -1;
		}
		if (unlink(buf))
		{
			strcpy(tmpname,buf);
		}
	}
#endif
	return 0;
}

static void ed_clos(void)
{
#ifdef TEMP_FP
	if (cur_file.t_fp)
	{
		fclose(cur_file.t_fp);
		cur_file.t_fp=NULL;
	}
#else
	if (cur_file.t_fd!=-1)
	{
		close(cur_file.t_fd);
		cur_file.t_fd=(-1);
	}
	if (tmpname[0])
	{
		if (unlink(tmpname))
		{
			perror(tmpname);
			exit(1);
		}
		tmpname[0]=0;
	}
#endif
	if (filename)
	{
		free(filename);
		filename=NULL;
	}
}

static int quit(void)
{
	mode='q';
	menu_erased=1;
	show_status();
	plot_cursor();
	fflush(stdout);

	while (forever) 
	{
		switch (getkey()) 
		{
		case 'e':
			update();
		case 'a':
			return 0;
		case 'u':
			update();
			return 1;
		case key_ins:
		case escape:
		case ctrl_z:
			/*		case key_home: */
			return 1;
		case 'w':
			{
				char *fname=enter_fname("Write file - ");
				menu_erased=1;
				if (fname && fname[0] && (fname != clip_name))
				{
					write_file(fname);
				}
				show_status();
				plot_cursor();
				fflush(stdout);
			}
			break;
		case 'i':
			{
				char *fname=enter_fname("Edit file - ");
				menu_erased=1;
				if (fname && fname[0] && (fname != clip_name))
				{
					ed_clos();
					ed_init();
					init(fname);
					return 1;
				}
				else
				{
					show_status();
					plot_cursor();
					fflush(stdout);
				}
			}
			break;
		}
	}

	return -1;
}

int main(int argc,char **argv)
{
	int editing=0;
	char *fn=NULL;
#ifdef HAVE_PWD_H
	struct passwd *pw=getpwuid(getuid());

	if (pw) 
	{
		clip_name=strjoin("/",pw->pw_dir,".aedit.clp",NULL);
	}
#else
#	ifdef _WIN32
	const char *homedir=getenv("USERPROFILE");
#	else
	const char *homedir=getenv("HOME");
#	endif
	if (homedir)
	{
		clip_name=strjoin(
#ifdef _WIN32
			"\\",
#else
			"/",
#endif
			homedir,".aedit.clp",NULL);
	}
#endif

	{
		char *p=getenv("COLUMNS");
		if (p && p[0]) total_cols=atol(p);
	}

	{
		char *p=getenv("LINES");
		if (p && p[0]) total_lines=atol(p);
	}

#ifdef SIGWINCH
	signal(SIGWINCH,winch);	/* want this to notify of screen changes */
#ifdef SIGTTIN
    aedit_siginterrupt(SIGWINCH,0); /* don't want system calls interrupted */
#endif
#endif

	if (tty_raw(0)<0)
	{
		fprintf(stderr,"Can't change stdin to RAW\n");
		return 1;
	}

	atexit(my_atexit);

	if (tty_sz(0))
	{
		char *p=getenv("LINES");
		if (p)
		{
			if (*p)
			{
				total_lines=atoi(p);
			}
		}
		p=getenv("COLUMNS");
		if (p)
		{
			if (*p)
			{
				total_cols=atoi(p);
			}
		}
	}

	if (ed_init())
	{
		fflush(stdout);
		my_atexit();
		return 1;
	}

	if (total_lines > max_lines)
	{
		total_lines=max_lines;
	}

	while (--argc) 
	{
		char *arg;
		argv++;
		arg=argv[0];
		if (arg[0]=='-') 
		{
			while (*++arg) 
			{
				switch (*arg) 
				{
				case 'v':
					total_lines=24;
					break;
				case 't':
					tabs=4; 
					break;
				case 'w':	
					wordwrap=0; 
					break;
				case 'm':
					rev_menu=0;
					break;
				}
			}
		} 
		else 
		{
			if (*arg=='+') 
			{
				while (*++arg) 
				{
					switch(*arg) 
					{
					case 'w':	
						wordwrap=1; 
						break;
					case 'm':
						rev_menu=1;
						break;
					case 't':
						tabs=8;
						break;
					case 'v':
						total_lines=25;
						break;
					}
				}
			} 
			else 
			{
				fn=arg;
			}
		}
	}

	plot(0,0);
	reverse(0);

	if (!init(fn))
	{
		editing=1;
	}

	while (editing) 
	{
		int c;
		if (again) 
		{
			c='a';
			again--;
		} 
		else 
		{
			c=getkey();
		}

		if (sel_on) 
		{
			sel_on=0;
			paint_text(sel_pos-ed_pos());
		}

		if ((c>='0') & (c<='9')) 
		{
			c=get_again(c);
		}

		switch (c) 
		{
		case nl:
			if (again) 
			{
				do_jump(again);
				again=0;
			}
			break;
		case key_ins:
			c='i';
		case 'i':
		case 'x':
			mode=c;
			insert_text();
			mode=0;
			menu_erased=1;
			show_status();
			plot_cursor();
			break;
		case 'd':
		case 'b':
		case key_sel:
			do_block();
			break;
		case tab:
			menu_id=menu_nxt;
			menu_erased=1;
			show_status();
			plot_cursor();
			break;
		case 'q':
			editing=quit();
			if (editing) 
			{
				mode=0;
				menu_erased=1;
				show_status();
				plot_cursor();
			}
			break;
		case ctrl_f:
		case key_find:
			c='f'; /* fallthrough */
		case 'f':
		case '-':
		case 'r':
		case '?':
			if (again) again--;
			last_cmd=(char)c;
			mode=c;
			do_find(c);
			break;
		case 'l':
			show_size=1;
			show_status();
			plot_cursor();
			break;
		case 'a':
			switch (last_cmd) 
			{
			case '?':
			case 'r':
			case '-':
			case 'f':
				do_find('a');
				break;
			default:
				again=0;
				break;
			}
			break;
		case 'g':
			do_get(enter_fname("Get text from - "));
			if (menu_erased) show_status();
			plot_cursor();
			break;
		case 'j':
			do_jump(again);
			again=0;
			break;
		case 's':
			do_shell();
			break;
		case 'v':
			c=ctrl_w;									/* fall through */
		default:
			if (!cur_key(c))
			{
			}
		}
		fflush(stdout);
	}

	ed_clos();

#ifdef MALLOC_SIZE
	if (cur_file.buffer) free(cur_file.buffer);
#endif

	plot(menu_line,0);
	clear_line();
	plot(menu_line,0);
	fflush(stdout);

	/* might be helpful to monitor output of socket
	   to see if any data hanging around
	   in transmit buffers, noticed when used rlogin, not telnet */

	my_atexit();
	return 0;
}

#ifdef _DEBUG
dumpinfo()
{
/*	printf("t_lol=%ld,",cur_file.t_lol);
	printf("low_size=%ld,",cur_file.low_size);
	printf("high_size=%ld,",cur_file.high_size);
	printf("t_hip=%ld,",cur_file.t_hip);
	printf("t_hil=%ld\n",cur_file.t_hil);*/
}
#endif
