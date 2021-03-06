/* $Id: chat.c 3918 2008-02-14 16:23:29Z piaip $ */
#include "bbs.h"

#ifndef DBCSAWARE
#define dbcs_off (1)
#endif

#define STOP_LINE (t_lines-3)
static int      chatline;
static FILE    *flog;
static void
printchatline(const char *str)
{
    move(chatline, 0);
    if (*str == '>' && !PERM_HIDE(currutmp))
	return;
    else if (chatline < STOP_LINE - 1)
	chatline++;
    else {
	region_scroll_up(2, STOP_LINE - 2);
	move(STOP_LINE - 2, 0);
    }
    outs(str);
    outc('\n');
    outs("→");

    if (flog)
	fprintf(flog, "%s\n", str);
}

static void
chat_clear(char*unused)
{
    for (chatline = 2; chatline < STOP_LINE; chatline++) {
	move(chatline, 0);
	clrtoeol();
    }
    move(b_lines, 0);
    clrtoeol();
    move(chatline = 2, 0);
    outs("→");
}

static void
print_chatid(const char *chatid)
{
    move(b_lines - 1, 0);
    clrtoeol();
    outs(chatid);
    outc(':');
}

static int
chat_send(int fd, const char *buf)
{
    int             len;
    char            genbuf[200];

    len = snprintf(genbuf, sizeof(genbuf), "%s\n", buf);
    return (send(fd, genbuf, len, 0) == len);
}

struct ChatBuf {
    char buf[128];
    int bufstart;
};

static int
chat_recv(struct ChatBuf *cb, int fd, char *chatroom, char *chatid, size_t chatid_size)
{
    int             c, len;
    char           *bptr;

    len = sizeof(cb->buf) - cb->bufstart - 1;
    if ((c = recv(fd, cb->buf + cb->bufstart, len, 0)) <= 0)
	return -1;
    c += cb->bufstart;

    bptr = cb->buf;
    while (c > 0) {
	len = strlen(bptr) + 1;
	if (len > c && (unsigned)len < (sizeof(cb->buf)/ 2) )
	    break;

	if (*bptr == '/') {
	    switch (bptr[1]) {
	    case 'c':
		chat_clear(NULL);
		break;
	    case 'n':
		strlcpy(chatid, bptr + 2, chatid_size);
		print_chatid(chatid);
		clrtoeol();
		break;
	    case 'r':
		strlcpy(chatroom, bptr + 2, IDLEN);
		break;
	    case 't':
		move(0, 0);
		clrtoeol();
		prints(ANSI_COLOR(1;37;46) " 談天室 [%-12s] " ANSI_COLOR(45) " 話題：%-48s" ANSI_RESET,
		       chatroom, bptr + 2);
	    }
	} else
	    printchatline(bptr);

	c -= len;
	bptr += len;
    }

    if (c > 0) {
	memmove(cb->buf, bptr, sizeof(cb->buf)-(bptr-cb->buf));
	cb->bufstart = len - 1;
    } else
	cb->bufstart = 0;
    return 0;
}

static void
chathelp(const char *cmd, const char *desc)
{
    char            buf[STRLEN];

    snprintf(buf, sizeof(buf), "  %-20s- %s", cmd, desc);
    printchatline(buf);
}

static void
chat_help(char *arg)
{
    if (strstr(arg, " op")) {
	printchatline("談天室管理員專用指令");
	chathelp("[/f]lag [+-][ls]", "設定鎖定、秘密狀態");
	chathelp("[/i]nvite <id>", "邀請 <id> 加入談天室");
	chathelp("[/k]ick <id>", "將 <id> 踢出談天室");
	chathelp("[/o]p <id>", "將 Op 的權力轉移給 <id>");
	chathelp("[/t]opic <text>", "換個話題");
	chathelp("[/w]all", "廣播 (站長專用)");
    } else {
	// chathelp("[/.]help", "chicken 鬥雞用指令");
	chathelp(" /help op", "談天室管理員專用指令");
	chathelp("[//]help", "MUD-like 社交動詞");
	chathelp("[/a]ct <msg>", "做一個動作");
	chathelp("[/b]ye [msg]", "道別");
	chathelp("[/c]lear", "清除螢幕");
	chathelp("[/j]oin <room>", "建立或加入談天室");
	chathelp("[/l]ist [room]", "列出談天室使用者");
	chathelp("[/m]sg <id> <msg>", "跟 <id> 說悄悄話");
	chathelp("[/n]ick <id>", "將談天代號換成 <id>");
	chathelp("[/p]ager", "切換呼叫器");
	chathelp("[/q]uery <id>", "查詢網友");
	chathelp("[/r]oom ", "列出一般談天室");
	chathelp("[/w]ho", "列出本談天室使用者");
	chathelp(" /whoin <room>", "列出談天室<room> 的使用者");
	chathelp(" /ignore <userid>", "忽略指定使用者的訊息");
	chathelp(" /unignore <userid>", "停止忽略指定使用者的訊息");
    }
}

static void
chat_date(char *unused)
{
    char            genbuf[200];

    snprintf(genbuf, sizeof(genbuf),
	     "◆ " BBSNAME "標準時間: %s", Cdate(&now));
    printchatline(genbuf);
}

static void
chat_pager(char *unused)
{
    char            genbuf[200];

    char           *msgs[PAGER_MODES] = {
	/* Ref: please match PAGER* in modes.h */
	"關閉", "打開", "拔掉", "防水", "好友"
    };

    snprintf(genbuf, sizeof(genbuf), "◆ 您的呼叫器:[%s]",
	     msgs[currutmp->pager = (currutmp->pager + 1) % PAGER_MODES]);
    printchatline(genbuf);
}

static void
chat_query(char *arg)
{
    char           *uid;
    int             tuid;
    userec_t        xuser;
    char *strtok_pos;

    printchatline("");
    strtok_r(arg, str_space, &strtok_pos);
    if ((uid = strtok_r(NULL, str_space, &strtok_pos)) && (tuid = getuser(uid, &xuser))) {
	char            buf[128], *ptr;
	FILE           *fp;

	snprintf(buf, sizeof(buf), "%s(%s) 共上站 %d 次，發表過 %d 篇文章",
		 xuser.userid, xuser.nickname,
		 xuser.numlogins, xuser.numposts);
	printchatline(buf);

	snprintf(buf, sizeof(buf),
		 "最近(%s)從[%s]上站", Cdate(&xuser.lastlogin),
		(xuser.lasthost[0] ? xuser.lasthost : "(不詳)"));
	printchatline(buf);

	sethomefile(buf, xuser.userid, fn_plans);
	if ((fp = fopen(buf, "r"))) {
	    tuid = 0;
	    while (tuid++ < MAX_QUERYLINES && fgets(buf, 128, fp)) {
		if ((ptr = strchr(buf, '\n')))
		    ptr[0] = '\0';
		printchatline(buf);
	    }
	    fclose(fp);
	}
    } else
	printchatline(err_uid);
}

typedef struct chat_command_t {
    char           *cmdname;	/* Chatroom command length */
    void            (*cmdfunc) (char *);	/* Pointer to function */
}               chat_command_t;

static const chat_command_t chat_cmdtbl[] = {
    {"help", chat_help},
    {"clear", chat_clear},
    {"date", chat_date},
    {"pager", chat_pager},
    {"query", chat_query},
    {NULL, NULL}
};

static int
chat_cmd_match(const char *buf, const char *str)
{
    while (*str && *buf && !isspace((int)*buf))
	if (tolower(*buf++) != *str++)
	    return 0;
    return 1;
}

static int
chat_cmd(char *buf, int fd)
{
    int             i;

    if (*buf++ != '/')
	return 0;

    for (i = 0; chat_cmdtbl[i].cmdname; i++) {
	if (chat_cmd_match(buf, chat_cmdtbl[i].cmdname)) {
	    chat_cmdtbl[i].cmdfunc(buf);
	    return 1;
	}
    }
    return 0;
}

#define MAXLASTCMD 6
static int      chatid_len = 10;
static time4_t lastEnter = 0;

int
t_chat(void)
{
    char     chatroom[IDLEN];/* Chat-Room Name */
    char            inbuf[80], chatid[20], lastcmd[MAXLASTCMD][80], *ptr = "";
    struct sockaddr_in sin;
    int             cfd, cmdpos, ch;
    int             currchar;
    int             chatting = YEA;
    char            fpath[80];
    struct ChatBuf chatbuf;

    if(HasUserPerm(PERM_VIOLATELAW))
    {
       vmsg("請先繳罰單才能使用聊天室!");
       return -1;
    }

    syncnow();

#ifdef CHAT_GAPMINS
    if ((now - lastEnter)/60 < CHAT_GAPMINS)
    {
       vmsg("您才剛離開聊天室，裡面正在整理中。請稍後再試。");
       return 0;
    }
#endif

#ifdef CHAT_REGDAYS
    if ((now - cuser.firstlogin)/86400 < CHAT_REGDAYS)
    {
       vmsg("您還不夠資深喔");
       return 0;
    }
#endif

    memset(&chatbuf, 0, sizeof(chatbuf));
    outs("                     驅車前往 請梢候........         ");

    memset(&sin, 0, sizeof sin);
#ifdef __FreeBSD__
    sin.sin_len = sizeof(sin);
#endif
    sin.sin_family = PF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    sin.sin_port = htons(NEW_CHATPORT);
    cfd = socket(sin.sin_family, SOCK_STREAM, 0);
    if (connect(cfd, (struct sockaddr *) & sin, sizeof sin) != 0) {
	outs("\n              "
	     "哇! 沒人在那邊耶...要有那地方的人先去開門啦!...");
	system("bin/xchatd");
	pressanykey();
	close(cfd);
	return -1;
    }

    while (1) {
	getdata(b_lines - 1, 0, "請輸入聊天代號：", chatid, 9, DOECHO);
	if(!chatid[0])
	    strlcpy(chatid, cuser.userid, sizeof(chatid));
	chatid[8] = '\0';
	/*
	 * 新格式:    /! UserID ChatID Password
	 */
	snprintf(inbuf, sizeof(inbuf), "/! %s %s %s",
		cuser.userid, chatid, cuser.passwd);
	chat_send(cfd, inbuf);
	if (recv(cfd, inbuf, 3, 0) != 3) {
	    close(cfd);
           vmsg("系統錯誤。");
	    return 0;
	}
	if (!strcmp(inbuf, CHAT_LOGIN_OK))
	    break;
	else if (!strcmp(inbuf, CHAT_LOGIN_EXISTS))
	    ptr = "這個代號已經有人用了";
	else if (!strcmp(inbuf, CHAT_LOGIN_INVALID))
	    ptr = "這個代號是錯誤的";
	else if (!strcmp(inbuf, CHAT_LOGIN_BOGUS))
	    ptr = "請勿派遣分身進入聊天室 !!";

	move(b_lines - 2, 0);
	outs(ptr);
	clrtoeol();
	bell();
    }
    syncnow();
    lastEnter = now;

    add_io(cfd, 0);

    currchar = 0;
    cmdpos = -1;
    memset(lastcmd, 0, sizeof(lastcmd));

    setutmpmode(CHATING);
    currutmp->in_chat = YEA;
    strlcpy(currutmp->chatid, chatid, sizeof(currutmp->chatid));

    clear();
    chatline = 2;

    move(STOP_LINE, 0);
    outs(msg_seperator);
    move(STOP_LINE, 56);
    outs(" /h 查詢指令  /b 離開 ");
    move(1, 0);
    outs(msg_seperator);
    print_chatid(chatid);
    memset(inbuf, 0, sizeof(inbuf));

    setuserfile(fpath, "chat_XXXXXX");
    flog = fdopen(mkstemp(fpath), "w");

    while (chatting) {
	move(b_lines - 1, currchar + chatid_len);
	ch = igetch();

	switch (ch) {
	case KEY_DOWN:
	    cmdpos += MAXLASTCMD - 2;
	case KEY_UP:
	    cmdpos++;
	    cmdpos %= MAXLASTCMD;
	    strlcpy(inbuf, lastcmd[cmdpos], sizeof(inbuf));
	    move(b_lines - 1, chatid_len);
	    clrtoeol();
	    outs(inbuf);
	    currchar = strlen(inbuf);
	    continue;
	case KEY_LEFT:
	    if (currchar)
	    {
		--currchar;
#ifdef DBCSAWARE
		if(currchar > 0 && 
			ISDBCSAWARE() &&
			getDBCSstatus((unsigned char*)inbuf, currchar) == DBCS_TRAILING)
		    currchar --;
#endif
	    }
	    continue;
	case KEY_RIGHT:
	    if (inbuf[currchar])
	    {
		++currchar;
#ifdef DBCSAWARE
		if(inbuf[currchar] &&
			ISDBCSAWARE() &&
			getDBCSstatus((unsigned char*)inbuf, currchar) == DBCS_TRAILING)
		    currchar++;
#endif
	    }
	    continue;
	case KEY_UNKNOWN:
	    continue;
	}

	if (ISNEWMAIL(currutmp)) {
	    printchatline("◆ 噹！郵差又來了...");
	}
	if (ch == I_OTHERDATA) {/* incoming */
	    if (chat_recv(&chatbuf, cfd, chatroom, chatid, 9) == -1) {
		chatting = chat_send(cfd, "/b");
		break;
	    }
	} else if (isprint2(ch)) {
	    if (currchar < 68) {
		if (inbuf[currchar]) {	/* insert */
		    int             i;

		    for (i = currchar; inbuf[i] && i < 68; i++);
		    inbuf[i + 1] = '\0';
		    for (; i > currchar; i--)
			inbuf[i] = inbuf[i - 1];
		} else		/* append */
		    inbuf[currchar + 1] = '\0';
		inbuf[currchar] = ch;
		move(b_lines - 1, currchar + chatid_len);
		outs(&inbuf[currchar++]);
	    }
	} else if (ch == '\n' || ch == '\r') {
	    if (*inbuf) {

#ifdef EXP_ANTIFLOOD
               // prevent flooding */
               static time4_t lasttime = 0;
               static int flood = 0;

               /* // debug anti flodding
               move(b_lines-3, 0); clrtoeol();
               prints("lasttime=%d, now=%d, flood=%d\n",
                       lasttime, now, flood);
               refresh();
               */
               syncnow();
               if (now - lasttime < 3 )
               {
                   // 3 秒內洗半面是不行的 ((25-5)/2)
                   if( ++flood > 10 ){
                       // flush all input!
                       drop_input();
                       while (wait_input(1, 0))
                       {
                           if (num_in_buf())
                               drop_input();
                           else
                               tty_read((unsigned char*)inbuf, sizeof(inbuf));
                       }
                       drop_input();
                       vmsg("請勿大量剪貼或造成洗板面的效果。");
                       // log?
                       sleep(2);
                       continue;
                   }
               } else {
                   lasttime = now;
                   flood = 0;
               }
#endif // anti-flood

		chatting = chat_cmd(inbuf, cfd);
		if (chatting == 0)
		    chatting = chat_send(cfd, inbuf);
		if (!strncmp(inbuf, "/b", 2))
		    break;

		for (cmdpos = MAXLASTCMD - 1; cmdpos; cmdpos--)
		    strlcpy(lastcmd[cmdpos],
			    lastcmd[cmdpos - 1], sizeof(lastcmd[cmdpos]));
		strlcpy(lastcmd[0], inbuf, sizeof(lastcmd[0]));

		inbuf[0] = '\0';
		currchar = 0;
		cmdpos = -1;
	    }
	    print_chatid(chatid);
	    move(b_lines - 1, chatid_len);
	} else if (ch == Ctrl('H') || ch == '\177') {
	    if (currchar) {
#ifdef DBCSAWARE
		int dbcs_off = 1;
		if (ISDBCSAWARE() && 
			getDBCSstatus((unsigned char*)inbuf, currchar-1) == DBCS_TRAILING)
		    dbcs_off = 2;
#endif
		currchar -= dbcs_off;
		inbuf[69] = '\0';
		memcpy(&inbuf[currchar], &inbuf[currchar + dbcs_off], 
			69 - currchar);
		move(b_lines - 1, currchar + chatid_len);
		clrtoeol();
		outs(&inbuf[currchar]);
	    }
	} else if (ch == Ctrl('Z') || ch == Ctrl('Y')) {
	    inbuf[0] = '\0';
	    currchar = 0;
	    print_chatid(chatid);
	    move(b_lines - 1, chatid_len);
	} else if (ch == Ctrl('C')) {
	    chat_send(cfd, "/b");
	    break;
	} else if (ch == Ctrl('D')) {
	    if ((size_t)currchar < strlen(inbuf)) {
#ifdef DBCSAWARE
		int dbcs_off = 1;
		if (ISDBCSAWARE() && inbuf[currchar+1] && 
			getDBCSstatus((unsigned char*)inbuf, currchar+1) == DBCS_TRAILING)
		    dbcs_off = 2;
#endif
		inbuf[69] = '\0';
		memcpy(&inbuf[currchar], &inbuf[currchar + dbcs_off], 
			69 - currchar);
		move(b_lines - 1, currchar + chatid_len);
		clrtoeol();
		outs(&inbuf[currchar]);
	    }
	} else if (ch == Ctrl('K')) {
	    inbuf[currchar] = 0;
	    move(b_lines - 1, currchar + chatid_len);
	    clrtoeol();
	} else if (ch == Ctrl('A')) {
	    currchar = 0;
	} else if (ch == Ctrl('E')) {
	    currchar = strlen(inbuf);
	} else if (ch == Ctrl('I')) {
	    screen_backup_t old_screen;

	    scr_dump(&old_screen);
	    add_io(0, 0);
	    t_idle();
	    scr_restore(&old_screen);
	    add_io(cfd, 0);
	} else if (ch == Ctrl('Q')) {
	    print_chatid(chatid);
	    move(b_lines - 1, chatid_len);
	    outs(inbuf);
	    continue;
	}
    }

    close(cfd);
    add_io(0, 0);
    currutmp->in_chat = currutmp->chatid[0] = 0;

    if (flog) {
	char            ans[4];

	fclose(flog);
	more(fpath, NA);
	getdata(b_lines - 1, 0, "清除(C) 移至備忘錄(M) (C/M)?[C]",
		ans, sizeof(ans), LCECHO);
	if (*ans == 'm') {
	    fileheader_t    mymail;
	    char            title[128];
	    char            genbuf[200];

	    sethomepath(genbuf, cuser.userid);
	    stampfile(genbuf, &mymail);
	    mymail.filemode = FILE_READ ;
	    strlcpy(mymail.owner, "[備.忘.錄]", sizeof(mymail.owner));
	    strlcpy(mymail.title, "會議" ANSI_COLOR(1;33) "記錄" ANSI_RESET, sizeof(mymail.title));
	    sethomedir(title, cuser.userid);
	    append_record(title, &mymail, sizeof(mymail));
	    Rename(fpath, genbuf);
	} else
	    unlink(fpath);
    }
    return 0;
}
