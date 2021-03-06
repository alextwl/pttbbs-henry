/* $Id: kaede.c 3819 2008-01-10 18:01:34Z piaip $ */
#include "bbs.h"

// TODO move stuff to libbbs(or util)/string.c, ...
// this file can be removed (or not?)

char           *
Ptt_prints(char *str, size_t size, int mode)
{
    char           *strbuf = alloca(size);
    int             r, w;
    for( r = w = 0 ; str[r] != 0 && w < (size - 1) ; ++r )
	if( str[r] != ESC_CHR )
	    strbuf[w++] = str[r];
	else{
	    if( str[++r] != '*' ){
		if(w+2>=size-1) break;
		strbuf[w++] = ESC_CHR;
		strbuf[w++] = str[r];
	    }
	    else{
		/* Note, w will increased by copied length after */
		switch( str[++r] ){

			// secure content
			
		case 't':	// current time
		    strlcpy(strbuf+w, Cdate(&now), size-w);
		    w += strlen(strbuf+w);
		    break;
		case 'u':	// current online users
		    w += snprintf(&strbuf[w], size - w,
				  "%d", SHM->UTMPnumber);
		    break;

			// insecure content
		
		case 's':	// current user id
		    strlcpy(strbuf+w, cuser.userid, size-w);
		    w += strlen(strbuf+w);
		    break;
		case 'n':	// current user nickname
		    strlcpy(strbuf+w, cuser.nickname, size-w);
		    w += strlen(strbuf+w);
		    break;
		case 'l':	// current user logins
		    w += snprintf(&strbuf[w], size - w,
				  "%d", cuser.numlogins);
		    break;
		case 'p':	// current user posts
		    w += snprintf(&strbuf[w], size - w,
				  "%d", cuser.numposts);
		    break;

		/* It's saver not to send these undefined escape string. 
		default:
		    strbuf[w++] = ESC_CHR;
		    strbuf[w++] = '*';
		    strbuf[w++] = str[r];
		    */
		}
	    }
	}
    strbuf[w] = 0;
    strip_ansi(str, strbuf, mode);
    return str;
}

// utility from screen.c
void
outs_n(const char *str, int n)
{
    while (*str && n--) {
	outc(*str++);
    }
}

// XXX left-right (for large term)
// TODO someday please add ANSI detection version
void 
outslr(const char *left, int leftlen, const char *right, int rightlen)
{
    if (left == NULL)
	left = "";
    if (right == NULL)
	right = "";
    if(*left && leftlen < 0)
	leftlen = strlen(left);
    if(*right && rightlen < 0)
	rightlen = strlen(right);
    // now calculate padding
    rightlen = t_columns - leftlen - rightlen;
    outs(left);

    // ignore right msg if we need to.
    if(rightlen >= 0)
    {
	while(--rightlen > 0)
	    outc(' ');
	outs(right);
    } else {
	rightlen = t_columns - leftlen;
	while(--rightlen > 0)
	    outc(' ');
    }
}


/* Jaky */
void
out_lines(const char *str, int line)
{
	int y, x;
	getyx(&y, &x);
    while (*str && line) {
		if (*str == '\n')
		{
			move(++y, 0);
			line--;
		} else 
		{
			outc(*str);
		}
		str++;
	}
}

void
outmsg(const char *msg)
{
    move(b_lines - msg_occupied, 0);
    clrtoeol();
    outs(msg);
}

void
outmsglr(const char *msg, int llen, const char *rmsg, int rlen)
{
    move(b_lines - msg_occupied, 0);
    clrtoeol();
    outslr(msg, llen, rmsg, rlen);
    outs(ANSI_RESET ANSI_CLRTOEND);
}

void
prints(const char *fmt,...)
{
    va_list         args;
    char            buff[1024];

    va_start(args, fmt);
    vsnprintf(buff, sizeof(buff), fmt, args);
    va_end(args);
    outs(buff);
}

void
mouts(int y, int x, const char *str)
{
    move(y, x);
    clrtoeol();
    outs(str);
}

// vim:ts=4
