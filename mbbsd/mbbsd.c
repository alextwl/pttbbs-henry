/* $Id: mbbsd.c 3941 2008-02-23 02:43:40Z piaip $ */
#include "bbs.h"
#include "banip.h"

#ifdef __linux__
#    ifdef CRITICAL_MEMORY
#        include <malloc.h>
#    endif
#    ifdef DEBUG
#        include <mcheck.h>
#    endif
#endif


#define SOCKET_QLEN 4

static void do_aloha(const char *hello);
static void getremotename(const struct sockaddr_in * from, char *rhost, char *rname);

#ifdef CONVERT
    #if !defined(__linux__)
void big2gb_init(void*);
void gb2big_init(void*);
void big2uni_init(void*);
void uni2big_init(void*);
    #endif
#endif

//////////////////////////////////////////////////////////////////
// Site Optimization
// override these macro if you need more optimization, 
// based on OS/lib/package...
#ifndef OPTIMIZE_LISTEN_SOCKET
#define OPTIMIZE_LISTEN_SOCKET(sock,sz)
#endif

#ifndef XAUTH_HOST
#define XAUTH_HOST(x) x
#endif

#ifndef XAUTH_GETREMOTENAME
#define XAUTH_GETREMOTENAME(x) x
#endif

#ifndef XAUTH_TRYREMOTENAME
#define XAUTH_TRYREMOTENAME()
#endif

#if 0
static jmp_buf  byebye;
#endif

static char     remoteusername[40] = "?";
static unsigned char enter_uflag;
static int      use_shell_login_mode = 0;
static int      listen_port = 23;

#ifdef DETECT_CLIENT
Fnv32_t client_code=FNV1_32_INIT;

void UpdateClientCode(unsigned char c)
{
    FNV1A_CHAR(c, client_code);
}
#endif

#ifdef USE_RFORK
#define fork() rfork(RFFDG | RFPROC | RFNOWAIT)
#endif

/* set signal handler, which won't be reset once signal comes */
static void
signal_restart(int signum, void (*handler) (int))
{
    struct sigaction act;
    act.sa_handler = handler;
    memset(&(act.sa_mask), 0, sizeof(sigset_t));
    act.sa_flags = 0;
    sigaction(signum, &act, NULL);
}

static void
start_daemon(void)
{
    int             n, fd;

    /*
     * More idiot speed-hacking --- the first time conversion makes the C
     * library open the files containing the locale definition and time zone.
     * If this hasn't happened in the parent process, it happens in the
     * children, once per connection --- and it does add up.
     */
    time_t          dummy = time(NULL);
    struct tm      *dummy_time = localtime(&dummy);
    char            buf[32];

    strftime(buf, sizeof(buf), "%d/%b/%Y:%H:%M:%S", dummy_time);

#ifndef NO_FORK
    if ((n = fork())) {
	exit(0);
    }
#endif

    /* rocker.011018: it's a good idea to close all unexcept fd!! */
#ifndef VALGRIND
    n = getdtablesize();
    while (n)
	close(--n);

    if( ((fd = open("log/stderr", O_WRONLY | O_CREAT | O_APPEND, 0644)) >= 0) && fd != 2 ){
	dup2(fd, 2);
	close(fd);
    }
#endif

    if(getenv("SSH_CLIENT"))
	unsetenv("SSH_CLIENT");

    /*
     * rocker.011018: we don't need to remember original tty, so request a
     * new session id
     */
    setsid();

    /*
     * rocker.011018: after new session, we should insure the process is
     * clean daemon
     */
#ifndef NO_FORK
    if ((n = fork())) {
	exit(0);
    }
#endif
}

static void
reapchild(int sig)
{
    int             state, pid;

    while ((pid = waitpid(-1, &state, WNOHANG | WUNTRACED)) > 0);
}

void
log_usies(const char *mode, const char *mesg)
{
    now = time(NULL);
    if (!mesg)
        log_filef(FN_USIES, LOG_CREAT, 
                 "%s %s %-12s Stay:%d (%s)\n",
                 Cdate(&now), mode, cuser.userid ,
                 (int)(now - login_start_time) / 60, cuser.nickname);
    else
        log_filef(FN_USIES, LOG_CREAT,
                 "%s %s %-12s %s\n",
                 Cdate(&now), mode, cuser.userid, mesg);

    /* 追蹤使用者 */
    if (HasUserPerm(PERM_LOGUSER))
        log_user("logout");
}


static void
setflags(int mask, int value)
{
    if (value)
	cuser.uflag |= mask;
    else
	cuser.uflag &= ~mask;
}

void
u_exit(const char *mode)
{
    int diff = (time(0) - login_start_time) / 60;
    int	dirty = currmode & MODE_DIRTY;

    currmode = 0;

    /* close fd 0 & 1 to terminate network */
    close(0);
    close(1);

    assert(strncmp(currutmp->userid,cuser.userid, IDLEN)==0);
    if(strncmp(currutmp->userid,cuser.userid, IDLEN)!=0)
	return;

    reload_money();
    /*
    cuser.goodpost = currutmp->goodpost;
    cuser.badpost = currutmp->badpost;
    cuser.goodsale = currutmp->goodsale;
    cuser.badsale = currutmp->badsale;
    */

    auto_backup();
    setflags(PAGER_FLAG, currutmp->pager != PAGER_ON);
    setflags(CLOAK_FLAG, currutmp->invisible);
    save_brdbuf();
    brc_finalize();

    cuser.invisible = currutmp->invisible;
    cuser.withme = currutmp->withme;
    cuser.pager = currutmp->pager;
    memcpy(cuser.mind, currutmp->mind, 4);
    setutmpbid(0);

    if (!SHM->GV2.e.shutdown) {
	if (!(HasUserPerm(PERM_SYSOP) && HasUserPerm(PERM_SYSOPHIDE)) &&
		!currutmp->invisible)
	    do_aloha("<<下站通知>> -- 我走囉！");
    }


    if ((cuser.uflag != enter_uflag) || dirty || diff) {
	if (!diff && cuser.numlogins)
	    cuser.numlogins = --cuser.numlogins;
	/* Leeym 上站停留時間限制式 */
    }
    passwd_update(usernum, &cuser);
    purge_utmp(currutmp);
    log_usies(mode, NULL);
}

void
abort_bbs(int sig)
{
    /* ignore normal signals */
    Signal(SIGALRM, SIG_IGN);
    Signal(SIGUSR1, SIG_IGN);
    Signal(SIGUSR2, SIG_IGN);
    Signal(SIGHUP, SIG_IGN);
    Signal(SIGTERM, SIG_IGN);
    Signal(SIGPIPE, SIG_IGN);
    if (currmode)
	u_exit("ABORTED");
    exit(0);
}

#ifdef GCC_NORETURN
static void abort_bbs_debug(int sig) GCC_NORETURN;
#endif

/* NOTE: It's better to use signal-safe functions. Avoid to call
 * functions with global/static variable -- data may be corrupted */
static void
abort_bbs_debug(int sig)
{
    int    i;
    sigset_t sigset;

    switch(sig) {
      case SIGINT: STATINC(STAT_SIGINT); break;
      case SIGQUIT: STATINC(STAT_SIGQUIT); break;
      case SIGILL: STATINC(STAT_SIGILL); break;
      case SIGABRT: STATINC(STAT_SIGABRT); break;
      case SIGFPE: STATINC(STAT_SIGFPE); break;
      case SIGBUS: STATINC(STAT_SIGBUS); break;
      case SIGSEGV: STATINC(STAT_SIGSEGV); break;
      case SIGXCPU: STATINC(STAT_SIGXCPU); break;
    }
    /* ignore normal signals */
    Signal(SIGALRM, SIG_IGN);
    Signal(SIGUSR1, SIG_IGN);
    Signal(SIGUSR2, SIG_IGN);
    Signal(SIGHUP, SIG_IGN);
    Signal(SIGTERM, SIG_IGN);
    Signal(SIGPIPE, SIG_IGN);

    /* unblock */
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGINT);
    sigaddset(&sigset, SIGQUIT);
    sigaddset(&sigset, SIGILL);
    sigaddset(&sigset, SIGABRT);
    sigaddset(&sigset, SIGFPE);
    sigaddset(&sigset, SIGBUS);
    sigaddset(&sigset, SIGSEGV);
    sigaddset(&sigset, SIGXCPU);
    sigprocmask(SIG_UNBLOCK, &sigset, NULL);

#define CRASH_MSG ANSI_COLOR(0) \
    "\r\n程式異常, 立刻斷線. \r\n" \
    "請洽 " GLOBAL_BUGREPORT " 板詳述問題發生經過。\r\n"

#define XCPU_MSG ANSI_COLOR(0) \
    "\r\n程式耗用過多計算資源, 立刻斷線。\r\n" \
    "可能是 (a)執行太多耗用資源的動作 或 (b)程式掉入無窮迴圈. "\
    "請洽 " GLOBAL_BUGREPORT " 板詳述問題發生經過。\r\n"

    if(sig==SIGXCPU)
	write(1, XCPU_MSG, sizeof(XCPU_MSG));
    else
	write(1, CRASH_MSG, sizeof(CRASH_MSG));

    /* close all file descriptors (including the network connection) */
    for (i = 0; i < 256; ++i)
	close(i);

    /* log */
    /* assume vsnprintf() in log_file() is signal-safe, is it? */
    log_filef("log/crash.log", LOG_CREAT, 
	    "%d %d %d %.12s\n", time4(NULL), getpid(), sig, cuser.userid);

    /* try logout... not a good idea, maybe crash again. now disabled */
    /*
    if (currmode) {
	currmode = 0;
	u_exit("AXXED");
    }
    */

#ifdef DEBUGSLEEP

#ifndef VALGRIND
    setproctitle("debug me!(%d)(%s,%d)", sig, cuser.userid, currstat);
#endif
    /* do this manually to prevent broken stuff */
    /* will broken currutmp cause problems here? hope not... */
    if(currutmp && strncmp(cuser.userid, currutmp->userid, IDLEN) == EQUSTR)
	currutmp->mode = DEBUGSLEEPING;

    sleep(DEBUGSLEEP_SECONDS);
#endif

    exit(0);
}

/* 登錄 BBS 程式 */
static void
mysrand(void)
{
    srandom(time(NULL) + getpid());	/* 時間跟 pid 當 rand 的 seed */
}

void
talk_request(int sig)
{
    STATINC(STAT_TALKREQUEST);
    bell();
    bell();
    if (currutmp->msgcount) {
	char            timebuf[100];

	syncnow();
	move(0, 0);
	clrtoeol();
	prints(ANSI_COLOR(33;41) "★%s" ANSI_COLOR(34;47) " [%s] %s " ANSI_COLOR(0) "",
		 SHM->uinfo[currutmp->destuip].userid, my_ctime(&now,timebuf,sizeof(timebuf)),
		 (currutmp->sig == 2) ? "重要消息廣播！(請Ctrl-U,l查看熱訊記錄)"
		 : "呼叫、呼叫，聽到請回答");
	refresh();
    } else {
	unsigned char   mode0 = currutmp->mode;
	char            c0 = currutmp->chatid[0];
	screen_backup_t old_screen;

	currutmp->mode = 0;
	currutmp->chatid[0] = 1;
	scr_dump(&old_screen);
	talkreply();
	currutmp->mode = mode0;
	currutmp->chatid[0] = c0;
	scr_restore(&old_screen);
    }
}

void
show_call_in(int save, int which)
{
    char            buf[200];
#ifdef PLAY_ANGEL
    if (currutmp->msgs[which].msgmode == MSGMODE_TOANGEL)
	snprintf(buf, sizeof(buf), ANSI_COLOR(1;37;46) "★%s" ANSI_COLOR(37;45) " %s " ANSI_RESET,
		currutmp->msgs[which].userid, currutmp->msgs[which].last_call_in);
    else
#endif
    snprintf(buf, sizeof(buf), ANSI_COLOR(1;33;46) "★%s" ANSI_COLOR(37;45) " %s " ANSI_RESET,
	     currutmp->msgs[which].userid, currutmp->msgs[which].last_call_in);
    outmsg(buf);

    if (save) {
	char            genbuf[200];
	if (!fp_writelog) {
	    sethomefile(genbuf, cuser.userid, fn_writelog);
	    fp_writelog = fopen(genbuf, "a");
	}
	if (fp_writelog) {
	    fprintf(fp_writelog, "%s [%s]\n", buf, Cdatelite(&now));
	}
    }
}

static int
add_history_water(water_t * w, const msgque_t * msg)
{
    memcpy(&w->msg[w->top], msg, sizeof(msgque_t));
    w->top++;
    w->top %= WATERMODE(WATER_OFO) ? 5 : MAX_REVIEW;

    if (w->count < MAX_REVIEW)
	w->count++;

    return w->count;
}

static int
add_history(const msgque_t * msg)
{
    int             i = 0, j, waterinit = 0;
    water_t        *tmp;
    check_water_init();
    if (WATERMODE(WATER_ORIG) || WATERMODE(WATER_NEW))
	add_history_water(&water[0], msg);
    if (WATERMODE(WATER_NEW) || WATERMODE(WATER_OFO)) {
	for (i = 0; i < 5 && swater[i]; i++)
	    if (swater[i]->pid == msg->pid
#ifdef PLAY_ANGEL
		    && swater[i]->msg[0].msgmode == msg->msgmode
		    /* When throwing waterball to angel directly */
#endif
	       	)
		break;
	if (i == 5) {
	    waterinit = 1;
	    i = 4;
	    memset(swater[4], 0, sizeof(water_t));
	} else if (!swater[i]) {
	    water_usies = i + 1;
	    swater[i] = &water[i + 1];
	    waterinit = 1;
	}
	tmp = swater[i];

	if (waterinit) {
	    memcpy(swater[i]->userid, msg->userid, sizeof(swater[i]->userid));
	    swater[i]->pid = msg->pid;
	}
	if (!swater[i]->uin)
	    swater[i]->uin = currutmp;

	for (j = i; j > 0; j--)
	    swater[j] = swater[j - 1];
	swater[0] = tmp;
	add_history_water(swater[0], msg);
    }
    if (WATERMODE(WATER_ORIG) || WATERMODE(WATER_NEW)) {
	if (watermode > 0 &&
	    (water_which == swater[0] || water_which == &water[0])) {
	    if (watermode < water_which->count)
		watermode++;
	    t_display_new();
	}
    }
    return i;
}

void
write_request(int sig)
{
    int             i, msgcount;

    STATINC(STAT_WRITEREQUEST);
#ifdef NOKILLWATERBALL
    if( reentrant_write_request ) /* kill again by shmctl */
	return;
    reentrant_write_request = 1;
#endif
    syncnow();
    check_water_init();
    if (WATERMODE(WATER_OFO)) {
	/* 如果目前正在回水球模式的話, 就不能進行 add_history() ,
	   因為會改寫 water[], 而使回水球目的爛掉, 所以分成幾種情況考慮.
	   sig != 0表真的有水球進來, 故顯示.
	   sig == 0表示沒有水球進來, 不過之前尚有水球還沒寫到 water[].
	*/
	static  int     alreadyshow = 0;

	if( sig ){ /* 真的有水球進來 */

	    /* 若原來正在 REPLYING , 則改成 RECVINREPLYING,
	       這樣在回水球結束後, 會再呼叫一次 write_request(0) */
	    if( wmofo == REPLYING )
		wmofo = RECVINREPLYING;

	    /* 顯示 */
	    for( ; alreadyshow < currutmp->msgcount && alreadyshow < MAX_MSGS
		     ; ++alreadyshow ){
		bell();
		show_call_in(1, alreadyshow);
		refresh();
	    }
	}

	/* 看看是不是要把 currutmp->msg 拿回 water[] (by add_history())
	   須要是不在回水球中 (NOTREPLYING) */
	if( wmofo == NOTREPLYING &&
	    (msgcount = currutmp->msgcount) > 0 ){
	    for( i = 0 ; i < msgcount ; ++i )
		add_history(&currutmp->msgs[i]);
	    if( (currutmp->msgcount -= msgcount) < 0 )
		currutmp->msgcount = 0;
	    alreadyshow = 0;
	}
    } else {
	if (currutmp->mode != 0 &&
	    currutmp->pager != PAGER_OFF &&
	    cuser.userlevel != 0 &&
	    currutmp->msgcount != 0 &&
	    currutmp->mode != TALK &&
	    currutmp->mode != EDITING &&
	    currutmp->mode != CHATING &&
	    currutmp->mode != PAGE &&
	    currutmp->mode != IDLE &&
	    currutmp->mode != MAILALL && currutmp->mode != MONITOR) {
	    char            c0 = currutmp->chatid[0];
	    int             currstat0 = currstat;
	    unsigned char   mode0 = currutmp->mode;

	    currutmp->mode = 0;
	    currutmp->chatid[0] = 2;
	    currstat = HIT;

#ifdef NOKILLWATERBALL
	    currutmp->wbtime = 0;
#endif
	    if( (msgcount = currutmp->msgcount) > 0 ){
		for( i = 0 ; i < msgcount ; ++i ){
		    bell();
		    show_call_in(1, 0);
		    add_history(&currutmp->msgs[0]);

		    if( (--currutmp->msgcount) < 0 )
			i = msgcount; /* force to exit for() */
		    else if( currutmp->msgcount > 0 )
			memmove(&currutmp->msgs[0],
				&currutmp->msgs[1],
				sizeof(msgque_t) * currutmp->msgcount);
		    igetch();
		}
	    }

	    currutmp->chatid[0] = c0;
	    currutmp->mode = mode0;
	    currstat = currstat0;
	} else {
	    bell();
	    show_call_in(1, 0);
	    add_history(&currutmp->msgs[0]);

	    refresh();
	    currutmp->msgcount = 0;
	}
    }
#ifdef NOKILLWATERBALL
    reentrant_write_request = 0;
    currutmp->wbtime = 0; /* race */
#endif
}

static userinfo_t*
getotherlogin(int num)
{
    userinfo_t *ui;
    do {
	if (!(ui = (userinfo_t *) search_ulistn(usernum, num)))
	    return NULL;		/* user isn't logged in */

	/* skip sleeping process, this is slow if lots */
	if(ui->mode == DEBUGSLEEPING)
	    num++;
	else if(ui->pid <= 0)
	    num++;
	else if(kill(ui->pid, 0) < 0)
	    num++;
	else
	    break;
    } while (1);

    return ui;
}

static void
multi_user_check(void)
{
    register userinfo_t *ui;
    char            genbuf[3];

    if (HasUserPerm(PERM_SYSOP))
	return;			/* don't check sysops */

    srandom(getpid());
    // race condition here, sleep may help..?
    if (cuser.userlevel) {
	usleep(random()%1000000); // 0~1s
	ui = getotherlogin(1);
	if(ui == NULL)
	    return;

	getdata(b_lines - 1, 0, "您想刪除其他重複的 login 嗎？[Y/n] ",
		genbuf, 3, LCECHO);

	usleep(random()%1000000);
	if (genbuf[0] != 'n') {
	    do {
		// scan again, old ui may be invalid
		ui = getotherlogin(1);
		if(ui==NULL)
		    return;
		if (ui->pid > 0) {
		    if(kill(ui->pid, SIGHUP)<0) {
			perror("kill SIGHUP fail");
			break;
		    }
		    log_usies("KICK ", cuser.nickname);
		}  else {
		    fprintf(stderr, "id=%s ui->pid=0\n", cuser.userid);
		}
		usleep(random()%2000000+1000000); // 1~3s
	    } while(getotherlogin(3) != NULL);
	} else {
	    /* deny login if still have 3 */
	    if (getotherlogin(3) != NULL)
		abort_bbs(0);	/* Goodbye(); */
	}
    } else {
	/* allow multiple guest user */
	if (search_ulistn(usernum, MAX_GUEST) != NULL) {
	    vmsg("抱歉，目前已有太多 guest 在站上, 請用new註冊。");
	    exit(1);
	}
    }
}

/* bad login */
static char * const str_badlogin = "logins.bad";

static void
logattempt(const char *uid, char type)
{
    char            fname[40];
    int             fd, len;
    char            genbuf[200];

    snprintf(genbuf, sizeof(genbuf), "%c%-12s[%s] %s@%s\n", type, uid,
	    Cdate(&login_start_time), remoteusername, fromhost);
    len = strlen(genbuf);
    if ((fd = open(str_badlogin, O_WRONLY | O_CREAT | O_APPEND, 0644)) > 0) {
	write(fd, genbuf, len);
	close(fd);
    }
    if (type == '-') {
	snprintf(genbuf, sizeof(genbuf),
		 "[%s] %s\n", Cdate(&login_start_time), fromhost);
	len = strlen(genbuf);
	sethomefile(fname, uid, str_badlogin);
	if ((fd = open(fname, O_WRONLY | O_CREAT | O_APPEND, 0644)) > 0) {
	    write(fd, genbuf, len);
	    close(fd);
	}
    }
}

void mkuserdir(const char *userid)
{
    char genbuf[200];
    sethomepath(genbuf, userid);
    // assume it is a dir, so just check if it is exist
    if (access(genbuf, F_OK) != 0)
	mkdir(genbuf, 0755);
}

static void
login_query(void)
{
#ifdef CONVERT
    /* uid 加一位, for gb login */
    char            uid[IDLEN + 2], passbuf[PASSLEN];
    int             attempts, len;
#else
    char            uid[IDLEN + 1], passbuf[PASSLEN];
    int             attempts;
#endif
    resolve_garbage();
    now = time(0);

#ifdef DEBUG
    move(1, 0);
    prints("debugging mode\ncurrent pid: %d\n", getpid());
#else
    show_file("etc/Welcome", 1, -1, NO_RELOAD);
#endif
    // XXX why output("1", 1); here?
    // this output has been here since rev 1...
    // output("1", 1);

    attempts = 0;
    while (1) {
	if (attempts++ >= LOGINATTEMPTS) {
	    more("etc/goodbye", NA);
	    pressanykey();
	    exit(1);
	}
	bzero(&cuser, sizeof(cuser));

#ifdef DEBUG
	move(19, 0);
	prints("current pid: %d ", getpid());
#endif

	if (getdata(20, 0, "請輸入代號，或以[guest]參觀，以[new]註冊: ",
		uid, sizeof(uid), DOECHO) < 1)
	{
	    // got nothing 
	    outs("請重新輸入。\n");
	    continue;
	}

#ifdef CONVERT
	/* switch to gb mode if uid end with '.' */
	len = strlen(uid);
	if (uid[0] && uid[len - 1] == '.') {
	    set_converting_type(CONV_GB);
	    uid[len - 1] = 0;
	    redrawwin();
	}
	else if (uid[0] && uid[len - 1] == ',') {
	    set_converting_type(CONV_UTF8);
	    uid[len - 1] = 0;
	    redrawwin();
	}
	else if (len >= IDLEN + 1)
	    uid[IDLEN] = 0;
#endif

	if (strcasecmp(uid, str_new) == 0) {
#ifdef LOGINASNEW
	    new_register();
	    mkuserdir(cuser.userid);
	    reginit_fav();
	    break;
#else
	    outs("本系統目前無法以 new 註冊, 請用 guest 進入\n");
	    continue;
#endif
	} else if (!is_validuserid(uid)) {

	    outs(err_uid);

	} else if (strcasecmp(uid, STR_GUEST) == 0) {	/* guest */

            if (initcuser(uid)< 1) exit (0) ;
	    cuser.userlevel = 0;
	    cuser.uflag = PAGER_FLAG | BRDSORT_FLAG | MOVIE_FLAG;
	    cuser.uflag2= 0; // we don't need FAVNEW_FLAG or anything else.

#ifdef GUEST_DEFAULT_DBCS_NOINTRESC
	    cuser.uflag |= DBCS_NOINTRESC;
#endif
	    // can we prevent mkuserdir() here?
	    mkuserdir(cuser.userid);
	    break;

	} else {

	    /* normal user */
	    getdata(21, 0, MSG_PASSWD,
		    passbuf, sizeof(passbuf), NOECHO);
	    passbuf[8] = '\0';

	    move (22, 0); clrtoeol();
	    outs("正在檢查密碼...");
	    move(22, 0); refresh(); 
	    /* prepare for later */
	    clrtoeol();

	    if( initcuser(uid) < 1 || !cuser.userid[0] ||
		!checkpasswd(cuser.passwd, passbuf) ){

		if(is_validuserid(cuser.userid))
		    logattempt(cuser.userid , '-');
		outs(ERR_PASSWD);

	    } else {

		logattempt(cuser.userid, ' ');
		outs("密碼正確！ 開始登入系統..."); 
		move(22, 0); refresh();
		clrtoeol();

		if (strcasecmp(str_sysop, cuser.userid) == 0){
#ifdef NO_SYSOP_ACCOUNT
		    exit(0);
#else /* 自動加上各個主要權限 */
		    cuser.userlevel = PERM_BASIC | PERM_CHAT | PERM_PAGE |
			PERM_POST | PERM_LOGINOK | PERM_MAILLIMIT |
			PERM_CLOAK | PERM_SEECLOAK | PERM_XEMPT |
			PERM_SYSOPHIDE | PERM_BM | PERM_ACCOUNTS |
			PERM_CHATROOM | PERM_BOARD | PERM_SYSOP | PERM_BBSADM;
#endif
		}
		/* 早該有 home 了, 不知道為何有的帳號會沒有, 被砍掉了? */
		mkuserdir(cuser.userid);
		break;
	    }
	}
    }
    multi_user_check();
#ifdef DETECT_CLIENT
    {
	int fd = open("log/client_code",O_WRONLY | O_CREAT | O_APPEND, 0644);
	if(fd>=0) {
	    write(fd, &client_code, sizeof(client_code));
	    close(fd);
	}
    }
#endif
}

void
add_distinct(const char *fname, const char *line)
{
    FILE           *fp;
    int             n = 0;

    if ((fp = fopen(fname, "a+"))) {
	char            buffer[80];
	char            tmpname[100];
	FILE           *fptmp;

	strlcpy(tmpname, fname, sizeof(tmpname));
	strcat(tmpname, "_tmp");
	if (!(fptmp = fopen(tmpname, "w"))) {
	    fclose(fp);
	    return;
	}
	rewind(fp);
	while (fgets(buffer, 80, fp)) {
	    char           *p = buffer + strlen(buffer) - 1;

	    if (p[-1] == '\n' || p[-1] == '\r')
		p[-1] = 0;
	    if (!strcmp(buffer, line))
		break;
	    sscanf(buffer + strlen(buffer) + 2, "%d", &n);
	    fprintf(fptmp, "%s%c#%d\n", buffer, 0, n);
	}

	if (feof(fp))
	    fprintf(fptmp, "%s%c#1\n", line, 0);
	else {
	    sscanf(buffer + strlen(buffer) + 2, "%d", &n);
	    fprintf(fptmp, "%s%c#%d\n", buffer, 0, n + 1);
	    while (fgets(buffer, 80, fp)) {
		sscanf(buffer + strlen(buffer) + 2, "%d", &n);
		fprintf(fptmp, "%s%c#%d\n", buffer, 0, n);
	    }
	}
	fclose(fp);
	fclose(fptmp);
	rename(tmpname, fname);
    }
}

void
del_distinct(const char *fname, const char *line, int casesensitive)
{
    FILE           *fp;
    int             n = 0;

    if ((fp = fopen(fname, "r"))) {
	char            buffer[80];
	char            tmpname[100];
	FILE           *fptmp;

	strlcpy(tmpname, fname, sizeof(tmpname));
	strcat(tmpname, "_tmp");
	if (!(fptmp = fopen(tmpname, "w"))) {
	    fclose(fp);
	    return;
	}
	rewind(fp);
	while (fgets(buffer, 80, fp)) {
	    char           *p = buffer + strlen(buffer) - 1;

	    if (p[-1] == '\n' || p[-1] == '\r')
		p[-1] = 0;
	    if(casesensitive)
	    {
		if (!strcmp(buffer, line))
		    break;
	    } else {
		if (!strcasecmp(buffer, line))
		    break;
	    }
	    sscanf(buffer + strlen(buffer) + 2, "%d", &n);
	    fprintf(fptmp, "%s%c#%d\n", buffer, 0, n);
	}

	if (!feof(fp))
	    while (fgets(buffer, 80, fp)) {
		sscanf(buffer + strlen(buffer) + 2, "%d", &n);
		fprintf(fptmp, "%s%c#%d\n", buffer, 0, n);
	    }
	fclose(fp);
	fclose(fptmp);
	rename(tmpname, fname);
    }
}

#ifdef WHERE
static int
where(const char *from)
{
    int i;

    for (i = 0; i < SHM->home_num; i++) {
	if ((SHM->home_ip[i] & SHM->home_mask[i]) == (ipstr2int(from) & SHM->home_mask[i])) {
	    return i;
	}
    }
    return 0;
}
#endif

static void
check_BM(void)
{
    /* XXX: -_- */
    int             i;

    cuser.userlevel &= ~PERM_BM;
    for( i = 0 ; i < numboards ; ++i )
	if( is_BM_cache(i + 1) ) /* XXXbid */
	    return;
    //for (i = 0, bhdr = bcache; i < numboards && !is_BM(bhdr->BM); i++, bhdr++);
}

static void
setup_utmp(int mode)
{
    /* NOTE, 在 getnewutmpent 之前不應該有任何 slow/blocking function */
    userinfo_t      uinfo;
    memset(&uinfo, 0, sizeof(uinfo));
    uinfo.pid = currpid = getpid();
    uinfo.uid = usernum;
    uinfo.mode = currstat = mode;

    uinfo.userlevel = cuser.userlevel;
    uinfo.sex = cuser.sex % 8;
    uinfo.lastact = time(NULL);
    strlcpy(uinfo.userid, cuser.userid, sizeof(uinfo.userid));
    //strlcpy(uinfo.realname, cuser.realname, sizeof(uinfo.realname));
    strlcpy(uinfo.nickname, cuser.nickname, sizeof(uinfo.nickname));
    strip_nonebig5((unsigned char *)uinfo.nickname, sizeof(uinfo.nickname));
    strlcpy(uinfo.from, fromhost, sizeof(uinfo.from));
    uinfo.five_win = cuser.five_win;
    uinfo.five_lose = cuser.five_lose;
    uinfo.five_tie = cuser.five_tie;
    uinfo.chc_win = cuser.chc_win;
    uinfo.chc_lose = cuser.chc_lose;
    uinfo.chc_tie = cuser.chc_tie;
    uinfo.chess_elo_rating = cuser.chess_elo_rating;
    uinfo.go_win = cuser.go_win;
    uinfo.go_lose = cuser.go_lose;
    uinfo.go_tie = cuser.go_tie;
    uinfo.invisible = cuser.invisible % 2;
    uinfo.pager = cuser.pager % PAGER_MODES;
    /*
    uinfo.goodpost = cuser.goodpost;
    uinfo.badpost = cuser.badpost;
    uinfo.goodsale = cuser.goodsale;
    uinfo.badsale = cuser.badsale;
    */
    if(cuser.withme & (cuser.withme<<1) & (WITHME_ALLFLAG<<1))
	cuser.withme = 0; /* unset all if contradict */
    uinfo.withme = cuser.withme & ~WITHME_ALLFLAG;
    memcpy(uinfo.mind, cuser.mind, 4);
    strip_nonebig5((unsigned char *)uinfo.mind, 4);
#ifdef WHERE
    uinfo.from_alias = where(fromhost);
#endif
#ifndef FAST_LOGIN
    setuserfile(buf, "remoteuser");

    strlcpy(remotebuf, fromhost, sizeof(fromhost));
    strcat(remotebuf, ctime4(&now));
    chomp(remotebuf);
    add_distinct(buf, remotebuf);
#endif
    if (enter_uflag & CLOAK_FLAG)
	uinfo.invisible = YEA;

#ifdef PLAY_ANGEL
    if (REJECT_QUESTION)
	uinfo.angel = 1;
    uinfo.angel |= ANGEL_STATUS() << 1;
#endif

    getnewutmpent(&uinfo);
    currmode = MODE_STARTED;
    SHM->UTMPneedsort = 1;
    // XXX 不用每 20 才檢查吧
    if (!(cuser.numlogins % 20) && cuser.userlevel & PERM_BM)
	check_BM();		/* Ptt 自動取下離職板主權力 */

#ifndef _BBS_UTIL_C_
    /* Very, very slow friend_load. */
    if( strcmp(cuser.userid, STR_GUEST) != 0 ) // guest 不處理好友
	friend_load(0);
    nice(3);
#endif
}

inline static void welcome_msg(void)
{
    prints(ANSI_RESET "      歡迎您第 " 
	    ANSI_COLOR(1;33) "%d" ANSI_COLOR(0;37) " 度拜訪本站，上次您是從 " 
	    ANSI_COLOR(1;33) "%s" ANSI_COLOR(0;37) " 連往本站，" 
	    ANSI_CLRTOEND "\n"
	    "     我記得那天是 " ANSI_COLOR(1;33) "%s" ANSI_COLOR(0;37) "。"
	    ANSI_CLRTOEND "\n"
	    ANSI_CLRTOEND "\n"
	    ,
	    ++cuser.numlogins, cuser.lasthost, Cdate(&(cuser.lastlogin)));
    pressanykey();
}

inline static void check_bad_login(void)
{
    char            genbuf[200];
    setuserfile(genbuf, str_badlogin);
    if (more(genbuf, NA) != -1) {
	move(b_lines - 3, 0);
	outs("通常並沒有辦法知道該ip是誰所有, "
		"以及其意圖(是不小心按錯或有意測您密碼)\n"
		"若您有帳號被盜用疑慮, 請經常更改您的密碼或使用加密連線");
	if (getans("您要刪除以上錯誤嘗試的記錄嗎? [y/N] ") == 'y')
	    unlink(genbuf);
    }
}

inline static void birthday_make_a_wish(const struct tm *ptime, const struct tm *tmp)
{
    if (tmp->tm_mday != ptime->tm_mday) {
	more("etc/birth.post", YEA);
	if (enter_board("Chat")==0) {
	    do_post();
	}
    }
}

inline static void record_lasthost(const char *fromhost)
{
    strlcpy(cuser.lasthost, fromhost, sizeof(cuser.lasthost));
}

inline static void check_mailbox_quota(void)
{
    if (chkmailbox())
	m_read();
}

static void init_guest_info(void)
{
    int i;
    char           *nick[13] = {
	"椰子", "貝殼", "內衣", "寶特瓶", "翻車魚",
	"樹葉", "浮萍", "鞋子", "潛水艇", "魔王",
	"鐵罐", "考卷", "大美女"
    };
    char           *name[13] = {
	"大王椰子", "鸚鵡螺", "比基尼", "可口可樂", "仰泳的魚",
	"憶", "高岡屋", "AIR Jordon", "紅色十月號", "批踢踢",
	"SASAYA椰奶", "鴨蛋", "布魯克鱈魚香絲"
    };
    char           *addr[13] = {
	"天堂樂園", "大海", "綠島小夜曲", "美國", "綠色珊瑚礁",
	"遠方", "原本海", "NIKE", "蘇聯", "男八618室",
	"愛之味", "天上", "藍色珊瑚礁"
    };
    i = login_start_time % 13;
    snprintf(cuser.nickname, sizeof(cuser.nickname),
	    "海邊漂來的%s", nick[(int)i]);
    strlcpy(currutmp->nickname, cuser.nickname,
	    sizeof(currutmp->nickname));
    strlcpy(cuser.realname, name[(int)i], sizeof(cuser.realname));
    strlcpy(cuser.address, addr[(int)i], sizeof(cuser.address));
    cuser.sex = i % 8;
    currutmp->pager = PAGER_DISABLE;
}

#if FOREIGN_REG_DAY > 0
inline static void foreign_warning(void){
    if ((cuser.uflag2 & FOREIGN) && !(cuser.uflag2 & LIVERIGHT)){
	if (login_start_time - cuser.firstlogin > (FOREIGN_REG_DAY - 5) * 24 * 3600){
	    mail_muser(cuser, "[出入境管理局]", "etc/foreign_expired_warn");
	}
	else if (login_start_time - cuser.firstlogin > FOREIGN_REG_DAY * 24 * 3600){
	    cuser.userlevel &= ~(PERM_LOGINOK | PERM_POST);
	    vmsg("警告：請至出入境管理局申請永久居留");
	}
    }
}
#endif


static void
user_login(void)
{
    struct tm       ptime, lasttime;
    int             nowusers, ifbirth = 0, i;

    /* NOTE! 在 setup_utmp 之前, 不應該有任何 blocking/slow function,
     * 否則可藉機 race condition 達到 multi-login */

    /* get local time */
    ptime = *localtime4(&now);
    
    /* 初始化: random number 增加user跟時間的差異 */
    mysrand();

    log_usies("ENTER", fromhost);
#ifndef VALGRIND
    setproctitle("%s: %s", margs, cuser.userid);
#endif
    resolve_fcache();
    /* resolve_boards(); */
    numboards = SHM->Bnumber;

    if(getenv("SSH_CLIENT") != NULL){
	struct sockaddr_in xsin;
	char frombuf[50];
	sscanf(getenv("SSH_CLIENT"), "%s", frombuf);
	xsin.sin_family = AF_INET;
	xsin.sin_port = htons(23);
	if (strrchr(frombuf, ':'))
	    inet_pton(AF_INET, strrchr(frombuf, ':') + 1, &xsin.sin_addr);
	else
	    inet_pton(AF_INET, frombuf, &xsin.sin_addr);
	getremotename(&xsin, fromhost, remoteusername);   /* RFC931 */
    }

    /* 初始化 uinfo、flag、mode */
    setup_utmp(LOGIN);
    enter_uflag = cuser.uflag;
    lasttime = *localtime4(&cuser.lastlogin);
    redrawwin();

    /* XUE's DENY LOGIN 08252007 */
    if (HasUserPerm(PERM_NOLOGIN)){
        vmsg("你的ID遭封印。");
        exit(1);
    }

    /* show welcome_login */
    if( (ifbirth = (ptime.tm_mday == cuser.day &&
		    ptime.tm_mon + 1 == cuser.month)) ){
	char buf[PATHLEN];
	snprintf(buf, sizeof(buf), "etc/Welcome_birth.%d", getHoroscope(cuser.month, cuser.day));
	more(buf, NA);
    }
    else {
#ifndef MULTI_WELCOME_LOGIN
	more("etc/Welcome_login", NA);
#else
	if( SHM->GV2.e.nWelcomes ){
	    char            buf[80];
	    snprintf(buf, sizeof(buf), "etc/Welcome_login.%d",
		     (int)login_start_time % SHM->GV2.e.nWelcomes);
	    more(buf, NA);
	}
#endif
    }
    refresh();
    currutmp->alerts |= load_mailalert(cuser.userid);

    if ((nowusers = SHM->UTMPnumber) > SHM->max_user) {
	SHM->max_user = nowusers;
	SHM->max_time = now;
    }

    if (!(HasUserPerm(PERM_SYSOP) && HasUserPerm(PERM_SYSOPHIDE)) &&
	!currutmp->invisible)
    {
	/* do_aloha is costly. do it later? */
	do_aloha("<<上站通知>> -- 我來啦！");
    }

    if (SHM->loginmsg.pid){
        if(search_ulist_pid(SHM->loginmsg.pid))
	    getmessage(SHM->loginmsg);
        else
	    SHM->loginmsg.pid=0;
    }

    if (cuser.userlevel) {	/* not guest */
	move(t_lines - 4, 0);
	clrtobot();
	welcome_msg();
	resolve_over18();

	if( ifbirth ){
	    birthday_make_a_wish(&ptime, &lasttime);
	    if( getans("是否要顯示「壽星」於使用者名單上？(y/N)") == 'y' )
		currutmp->birth = 1;
	}
	check_bad_login();
	check_mailbox_quota();
	check_register();
	record_lasthost(fromhost);
	restore_backup();

    } else if (strcmp(cuser.userid, STR_GUEST) == 0) { /* guest */

	init_guest_info();
#if 0 // def DBCSAWARE
	u_detectDBCSAwareEvilClient();
#else
	pressanykey();
#endif
    } else {
	// XXX no userlevel, no guest - what is this?
	// clear();
	// outs("此帳號停權中");
	// pressanykey();
	// exit(1);

	check_mailbox_quota();
    }

    if(ptime.tm_yday!=lasttime.tm_yday)
	STATINC(STAT_TODAYLOGIN_MAX);

    if (!PERM_HIDE(currutmp)) {
	/* If you wanna do incremental upgrade
	 * (like, added a function/flag that wants user to confirm againe)
	 * put it here.
	 */

#if defined(DBCSAWARE) && defined(DBCSAWARE_UPGRADE_STARTTIME)
	// define the real time you upgraded in your pttbbs.conf
	if(cuser.lastlogin < DBCSAWARE_UPGRADE_STARTTIME)
	{
	    if (u_detectDBCSAwareEvilClient())
		cuser.uflag &= ~DBCSAWARE_FLAG;
	    else
		cuser.uflag |= DBCSAWARE_FLAG;
	}
#endif
	/* login time update */

	if(ptime.tm_yday!=lasttime.tm_yday)
	    STATINC(STAT_TODAYLOGIN_MIN);


	cuser.lastlogin = login_start_time;

    }

#if FOREIGN_REG_DAY > 0
    foreign_warning();
#endif

    passwd_update(usernum, &cuser);

    if(cuser.uflag2 & FAVNEW_FLAG) {
	fav_load();
	if (get_fav_root() != NULL) {
	    int num;
	    num = updatenewfav(1);
	    if (num > NEW_FAV_THRESHOLD &&
		getans("找到 %d 個新看板，確定要加入我的最愛嗎？[Y/n]", num) == 'n') {
		fav_free();
		fav_load();
	    }
	}
    }

    for (i = 0; i < NUMVIEWFILE; i++)
	if ((cuser.loginview >> i) & 1)
	    more(loginview_file[(int)i][0], YEA);
}

static void
do_aloha(const char *hello)
{
    FILE           *fp;
    char            userid[80];
    char            genbuf[200];

    setuserfile(genbuf, "aloha");
    if ((fp = fopen(genbuf, "r"))) {
	while (fgets(userid, 80, fp)) {
	    userinfo_t     *uentp;
	    if ((uentp = (userinfo_t *) search_ulist_userid(userid)) && 
		    isvisible(uentp, currutmp)) {
		my_write(uentp->pid, hello, uentp->userid, WATERBALL_ALOHA, uentp);
	    }
	}
	fclose(fp);
    }
}

static void
do_term_init(void)
{
    term_init();
    initscr();
    if(use_shell_login_mode)
	raise(SIGWINCH);
}

inline static void
start_client(void)
{
#ifdef CPULIMIT
    struct rlimit   rml;
    rml.rlim_cur = CPULIMIT * 60 - 5;
    rml.rlim_max = CPULIMIT * 60;
    setrlimit(RLIMIT_CPU, &rml);
#endif

    STATINC(STAT_LOGIN);
    /* system init */
    nice(2);			/* Ptt: lower priority */
    login_start_time = time(0);
    currmode = 0;

    Signal(SIGHUP, abort_bbs);
    Signal(SIGTERM, abort_bbs);
    Signal(SIGPIPE, abort_bbs);

    Signal(SIGINT, abort_bbs_debug);
    Signal(SIGQUIT, abort_bbs_debug);
    Signal(SIGILL, abort_bbs_debug);
    Signal(SIGABRT, abort_bbs_debug);
    Signal(SIGFPE, abort_bbs_debug);
    Signal(SIGBUS, abort_bbs_debug);
    Signal(SIGSEGV, abort_bbs_debug);
    Signal(SIGXCPU, abort_bbs_debug);

    signal_restart(SIGUSR1, talk_request);
    signal_restart(SIGUSR2, write_request);

    dup2(0, 1);

    do_term_init();
    Signal(SIGALRM, abort_bbs);
    alarm(600);

    login_query();		/* Ptt 加上login time out */
    m_init();			/* init the user mail path */
    user_login();
    auto_close_polls();		/* 自動開票 */

    Signal(SIGALRM, SIG_IGN);
    main_menu();
}

/* 取得 remote user name 以判定身份                */
/*
 * rfc931() speaks a common subset of the RFC 931, AUTH, TAP, IDENT and RFC
 * 1413 protocols. It queries an RFC 931 etc. compatible daemon on a remote
 * host to look up the owner of a connection. The information should not be
 * used for authentication purposes. This routine intercepts alarm signals.
 * 
 * Author: Wietse Venema, Eindhoven University of Technology, The Netherlands.
 */

#define RFC931_TIMEOUT   10
#define RFC931_PORT     113	/* Semi-well-known port */
#define ANY_PORT        0	/* Any old port will do */

#if 0
/* timeout - handle timeouts */
static void
timeout(int sig)
{
    longjmp(byebye, sig);
}
#endif

static void
getremotename(const struct sockaddr_in * from, char *rhost, char *rname)
{

    /* get remote host name */

#ifdef FAST_LOGIN
    XAUTH_HOST(strcpy(rhost, (char *)inet_ntoa(from->sin_addr)));
#else
    struct sockaddr_in our_sin;
    struct sockaddr_in rmt_sin;
    unsigned        rmt_port, rmt_pt;
    unsigned        our_port, our_pt;
    FILE           *fp;
    char            buffer[512], user[80], *cp;
    int             s;
    static struct hostent *hp;


    hp = NULL;
    if (setjmp(byebye) == 0) {
	Signal(SIGALRM, timeout);
	alarm(3);
	hp = gethostbyaddr((char *)&from->sin_addr, sizeof(struct in_addr),
			   from->sin_family);
	alarm(0);
    }
    strcpy(rhost, hp ? hp->h_name : (char *)inet_ntoa(from->sin_addr));

    /*
     * Use one unbuffered stdio stream for writing to and for reading from
     * the RFC931 etc. server. This is done because of a bug in the SunOS
     * 4.1.x stdio library. The bug may live in other stdio implementations,
     * too. When we use a single, buffered, bidirectional stdio stream ("r+"
     * or "w+" mode) we read our own output. Such behaviour would make sense
     * with resources that support random-access operations, but not with
     * sockets.
     */

    s = sizeof(our_sin);
    if (getsockname(0, (struct sockaddr *) & our_sin, &s) < 0)
	return;

    if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	return;

    if (!(fp = fdopen(s, "r+"))) {
	close(s);
	return;
    }
    /* Set up a timer so we won't get stuck while waiting for the server. */
    if (setjmp(byebye) == 0) {
	Signal(SIGALRM, timeout);
	alarm(RFC931_TIMEOUT);

	/*
	 * Bind the local and remote ends of the query socket to the same IP
	 * addresses as the connection under investigation. We go through all
	 * this trouble because the local or remote system might have more
	 * than one network address. The RFC931 etc. client sends only port
	 * numbers; the server takes the IP addresses from the query socket.
	 */
	our_pt = ntohs(our_sin.sin_port);
	our_sin.sin_port = htons(ANY_PORT);

	rmt_sin = *from;
	rmt_pt = ntohs(rmt_sin.sin_port);
	rmt_sin.sin_port = htons(RFC931_PORT);

	setbuf(fp, (char *)0);
	s = fileno(fp);

	if (bind(s, (struct sockaddr *) & our_sin, sizeof(our_sin)) >= 0 &&
	  connect(s, (struct sockaddr *) & rmt_sin, sizeof(rmt_sin)) >= 0) {
	    /*
	     * Send query to server. Neglect the risk that a 13-byte write
	     * would have to be fragmented by the local system and cause
	     * trouble with buggy System V stdio libraries.
	     */
	    fprintf(fp, "%u,%u\r\n", rmt_pt, our_pt);
	    fflush(fp);
	    /*
	     * Read response from server. Use fgets()/sscanf() so we can work
	     * around System V stdio libraries that incorrectly assume EOF
	     * when a read from a socket returns less than requested.
	     */
	    if (fgets(buffer, sizeof(buffer), fp) && !ferror(fp)
		&& !feof(fp)
		&& sscanf(buffer, "%u , %u : USERID :%*[^:]:%79s", &rmt_port,
			  &our_port, user) == 3 && rmt_pt == rmt_port
		&& our_pt == our_port) {

		/*
		 * Strip trailing carriage return. It is part of the
		 * protocol, not part of the data.
		 */
		if ((cp = (char *)strchr(user, '\r')))
		    *cp = 0;
		strlcpy(rname, user, sizeof(user));
	    }
	}
	alarm(0);
    }
    fclose(fp);
#endif
}

static int
bind_port(int port)
{
    int             sock, on, sz;
    struct linger   lin;
    struct sockaddr_in xsin;

    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    on = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on));
    setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (char *)&on, sizeof(on));

    lin.l_onoff = 0;
    setsockopt(sock, SOL_SOCKET, SO_LINGER, &lin, sizeof(lin));

    sz = 1024;
    setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (void*)&sz, sizeof(sz));
    sz = 4096;
    setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (void*)&sz, sizeof(sz));

    OPTIMIZE_LISTEN_SOCKET(sock, sz);

    xsin.sin_family = AF_INET;
    xsin.sin_addr.s_addr = htonl(INADDR_ANY);
    xsin.sin_port = htons(port);
    if (bind(sock, (struct sockaddr *) & xsin, sizeof xsin) < 0) {
	syslog(LOG_INFO, "bbsd bind_port can't bind to %d", port);
	exit(1);
    }
    if (listen(sock, SOCKET_QLEN) < 0) {
	syslog(LOG_INFO, "bbsd bind_port can't listen to %d", port);
	exit(1);
    }
    return sock;
}


/*******************************************************/


static int      shell_login(int argc, char *argv[], char *envp[]);
static int      daemon_login(int argc, char *argv[], char *envp[]);
static int      check_ban_and_load(int fd);
static int      check_banip(char *host);

int
main(int argc, char *argv[], char *envp[])
{
    start_time = time(NULL);

    /* avoid SIGPIPE */
    Signal(SIGPIPE, SIG_IGN);

    /* avoid erroneous signal from other mbbsd */
    Signal(SIGUSR1, SIG_IGN);
    Signal(SIGUSR2, SIG_IGN);

#if defined(__GLIBC__) && defined(CRITICAL_MEMORY)
    #define MY__MMAP_THRESHOLD (1024 * 8)
    #define MY__MMAP_MAX (0)
    #define MY__TRIM_THRESHOLD (1024 * 8)
    #define MY__TOP_PAD (0)

    mallopt (M_MMAP_THRESHOLD, MY__MMAP_THRESHOLD);
    mallopt (M_MMAP_MAX, MY__MMAP_MAX);
    mallopt (M_TRIM_THRESHOLD, MY__TRIM_THRESHOLD);
    mallopt (M_TOP_PAD, MY__TOP_PAD);
#endif
    
    attach_SHM();
    if( (argc == 3 && shell_login(argc, argv, envp)) ||
	(argc != 3 && daemon_login(argc, argv, envp)) )
	start_client();

    return 0;
}

static int
shell_login(int argc, char *argv[], char *envp[])
{
    int fd;

    STATINC(STAT_SHELLLOGIN);
    /* Give up root privileges: no way back from here */
    setgid(BBSGID);
    setuid(BBSUID);
    chdir(BBSHOME);

#if defined(linux) && defined(DEBUG)
//    mtrace();
#endif

    use_shell_login_mode = 1;
    initsetproctitle(argc, argv, envp);

    snprintf(margs, sizeof(margs), "%s ssh ", argv[0]);
    /*
     * copy fromindent: Standard input:1138: Error:Unexpected end of file the
     * original "bbs"
     */
    if (argc > 1) {
	strcpy(fromhost, argv[1]);
	if (argc > 3)
	    strlcpy(remoteusername, argv[3], sizeof(remoteusername));
    }
    close(2);
    /* don't close fd 1, at least init_tty need it */
    if( ((fd = open("log/stderr", O_WRONLY | O_CREAT | O_APPEND, 0644)) >= 0) && fd != 2 ){
	dup2(fd, 2);
	close(fd);
    }

    init_tty();
    if (check_ban_and_load(0)) {
	sleep(10);
	return 0;
    }
#ifdef DETECT_CLIENT
    FNV1A_CHAR(123, client_code);
#endif
    return 1;
}

static int
daemon_login(int argc, char *argv[], char *envp[])
{
    int             msock, csock;	/* socket for Master and Child */
    FILE           *fp;
    int             len_of_sock_addr, overloading = 0, i;
    char            buf[256];
#if OVERLOADBLOCKFDS
    int             blockfd[OVERLOADBLOCKFDS];
    int             nblocked = 0;
#endif
    struct sockaddr_in xsin;
    xsin.sin_family = AF_INET;

    /* setup standalone */
    start_daemon();
    signal_restart(SIGCHLD, reapchild);

    /* choose port */
    if( argc < 2 )
	listen_port = 3006;
    else{
#ifdef NO_FORK
	listen_port = atoi(argv[1]);
#else
	for( i = 1 ; i < (argc - 1) ; ++i )
	    switch( fork() ){
	    case -1:
		perror("fork()");
		break;
	    case 0:
		goto out;
	    default:
		break;
	    }
      out:
	listen_port = atoi(argv[i]);
#endif
    }

    /* port binding */
    if( (msock = bind_port(listen_port)) < 0 ){
	syslog(LOG_INFO, "mbbsd bind_port failed.\n");
	exit(1);
    }

    /* Give up root privileges: no way back from here */
    setgid(BBSGID);
    setuid(BBSUID);
    chdir(BBSHOME);

    /* proctitle */
    initsetproctitle(argc, argv, envp);
#ifndef VALGRIND
    snprintf(margs, sizeof(margs), "%s %d ", argv[0], listen_port);
    setproctitle("%s: listening ", margs);
#endif

    /* It's better to do something before fork */
#ifdef CONVERT
    big2gb_init(NULL);
    gb2big_init(NULL);
    big2uni_init(NULL);
    uni2big_init(NULL);
#endif
    
#ifndef NO_FORK
#ifdef PRE_FORK
    if( listen_port == 23 ){ // only pre-fork in port 23
	for( i = 0 ; i < PRE_FORK ; ++i )
	    if( fork() <= 0 )
		break;
    }
#endif
#endif

    snprintf(buf, sizeof(buf),
	     "run/mbbsd.%d.%d.pid", listen_port, (int)getpid());
    if ((fp = fopen(buf, "w"))) {
	fprintf(fp, "%d\n", (int)getpid());
	fclose(fp);
    }

    /* main loop */
    while( 1 ){
	len_of_sock_addr = sizeof(xsin);
	if( 
#if defined(Solaris) && __OS_MAJOR_VERSION__ == 5 && __OS_MINOR_VERSION__ < 7
	   (csock = accept(msock, (struct sockaddr *)&xsin,
			   &len_of_sock_addr)) < 0
#else
	   (csock = accept(msock, (struct sockaddr *)&xsin,
			   (socklen_t *)&len_of_sock_addr)) < 0 
#endif
	   ) {
	    if (errno != EINTR)
		sleep(1);
	    continue;
	}

	XAUTH_TRYREMOTENAME();

	overloading = check_ban_and_load(csock);
#if OVERLOADBLOCKFDS
	if( (!overloading && nblocked) ||
	    (overloading && nblocked == OVERLOADBLOCKFDS) ){
	    for( i = 0 ; i < OVERLOADBLOCKFDS ; ++i )
		if( blockfd[i] != csock && blockfd[i] != msock )
		    /* blockfd[i] should not be msock, but it happened */
		    close(blockfd[i]);
	    nblocked = 0;
	}
#endif

	if( overloading ){
#if OVERLOADBLOCKFDS
	    blockfd[nblocked++] = csock;
#else
	    close(csock);
#endif
	    continue;
	}

#ifdef NO_FORK
	break;
#else
	if (fork() == 0)
	    break;
	else
	    close(csock);
#endif
    }
    /* here is only child running */

#ifndef VALGRIND
    setproctitle("%s: ...login wait... ", margs);
#endif
    close(msock);
    dup2(csock, 0);
    close(csock);

    XAUTH_GETREMOTENAME(getremotename(&xsin, fromhost, remoteusername));

    if( check_banip(fromhost) ){
	sleep(10);
	exit(0);
    }
    telnet_init();
    return 1;
}

/*
 * check if we're banning login and if the load is too high. if login is
 * permitted, return 0; else return -1; approriate message is output to fd.
 */
static int
check_ban_and_load(int fd)
{
    FILE           *fp;
    static time4_t   chkload_time = 0;
    static int      overload = 0;	/* overload or banned, update every 1
					 * sec  */
    static int      banned = 0;

#ifdef INSCREEN
    write(fd, INSCREEN, sizeof(INSCREEN));
#else
#define BANNER \
"【" BBSNAME "】◎ 台大流行網 ◎(" MYHOSTNAME ") 調幅(" MYIP ") \r\n"
    write(fd, BANNER, sizeof(BANNER));
#endif

    if ((time(0) - chkload_time) > 1) {
	overload = 0;
	banned = 0;

	if(cpuload(NULL) > MAX_CPULOAD)
	    overload = 1;
	else if (SHM->UTMPnumber >= MAX_ACTIVE
#ifdef DYMAX_ACTIVE
		|| (SHM->GV2.e.dymaxactive > 2000 &&
		    SHM->UTMPnumber >= SHM->GV2.e.dymaxactive)
#endif
		) {
	    ++SHM->GV2.e.toomanyusers;
	    overload = 2;
	} else if(!access(BBSHOME "/" BAN_FILE, R_OK))
	    banned = 1;

	chkload_time = time(0);
    }

    if(overload == 1)
	write(fd, "系統過載, 請稍後再來\r\n", 22);
    else if(overload == 2)
	write(fd, "由於人數過多，請您稍後再來。", 28);
    else if (banned && (fp = fopen(BBSHOME "/" BAN_FILE, "r"))) {
	char     buf[256];
	while (fgets(buf, sizeof(buf), fp))
	    write(fd, buf, strlen(buf));
	fclose(fp);
    }

    if (banned || overload)
	return -1;

    return 0;
}

static int check_banip(char *host)
{
    unsigned int thisip = 0;
    char *ptr, *myhost = strdup(host);
    char *strtok_pos;

    for( ptr = strtok_r(myhost, ".", &strtok_pos) ; ptr != NULL ; ptr = strtok_r(NULL, ".", &strtok_pos) )
	thisip = thisip * 256 + atoi(ptr);
    free(myhost);

    return uintbsearch(thisip, &banip[1], banip[0]) ? 1 : 0;
}

/* vim: sw=4 
 */
