/* $Id: cache.c 3948 2008-02-25 16:35:38Z piaip $ */
#include "bbs.h"

#ifdef _BBS_UTIL_C_
#    define log_usies(a, b) ;
#    define abort_bbs(a)    exit(1)
#endif
/*
 * the reason for "safe_sleep" is that we may call sleep during SIGALRM
 * handler routine, while SIGALRM is blocked. if we use the original sleep,
 * we'll never wake up.
 */
unsigned int
safe_sleep(unsigned int seconds)
{
    /* jochang  sleep有問題時用 */
    sigset_t        set, oldset;

    sigemptyset(&set);
    sigprocmask(SIG_BLOCK, &set, &oldset);
    if (sigismember(&oldset, SIGALRM)) {
	unsigned int    retv;
	log_usies("SAFE_SLEEP ", "avoid hang");
	sigemptyset(&set);
	sigaddset(&set, SIGALRM);
	sigprocmask(SIG_UNBLOCK, &set, NULL);
	retv = sleep(seconds);
	sigprocmask(SIG_BLOCK, &set, NULL);
	return retv;
    }
    return sleep(seconds);
}

/*
 * section - SHM
 */
static void
attach_err(int shmkey, const char *name)
{
    fprintf(stderr, "[%s error] key = %x\n", name, shmkey);
    fprintf(stderr, "errno = %d: %s\n", errno, strerror(errno));
    exit(1);
}

void           *
attach_shm(int shmkey, int shmsize)
{
    void           *shmptr = (void *)NULL;
    int             shmid;

    shmid = shmget(shmkey, shmsize,
#ifdef USE_HUGETLB
	    SHM_HUGETLB |
#endif
	    0);
    if (shmid < 0) {
	// SHM should be created by uhash_loader, NOT mbbsd or other utils
	attach_err(shmkey, "shmget");
    } else {
	shmptr = (void *)shmat(shmid, NULL, 0);
	if (shmptr == (void *)-1)
	    attach_err(shmkey, "shmat");
    }

    return shmptr;
}

void
attach_SHM(void)
{
    SHM = attach_shm(SHM_KEY, SHMSIZE);
    if(SHM->version != SHM_VERSION) {
      fprintf(stderr, "Error: SHM->version(%d) != SHM_VERSION(%d)\n", SHM->version, SHM_VERSION);
      fprintf(stderr, "Please use the source code version corresponding to SHM,\n"
	 "or use ipcrm(1) command to clean share memory.\n");
      exit(1);
    }
    if (!SHM->loaded)		/* (uhash) assume fresh shared memory is
				 * zeroed */
	exit(1);
    if (SHM->Btouchtime == 0)
	SHM->Btouchtime = 1;
    bcache = SHM->bcache;
    numboards = SHM->Bnumber;

    if (SHM->Ptouchtime == 0)
	SHM->Ptouchtime = 1;

    if (SHM->Ftouchtime == 0)
	SHM->Ftouchtime = 1;
}

/* ----------------------------------------------------- */
/* semaphore : for critical section                      */
/* ----------------------------------------------------- */
#define SEM_FLG        0600	/* semaphore mode */

#ifndef __FreeBSD__
/* according to X/OPEN, we have to define it ourselves */
union semun {
    int             val;	/* value for SETVAL */
    struct semid_ds *buf;	/* buffer for IPC_STAT, IPC_SET */
    unsigned short int *array;	/* array for GETALL, SETALL */
    struct seminfo *__buf;	/* buffer for IPC_INFO */
};
#endif

void
sem_init(int semkey, int *semid)
{
    union semun     s;

    s.val = 1;
    *semid = semget(semkey, 1, 0);
    if (*semid == -1) {
	*semid = semget(semkey, 1, IPC_CREAT | SEM_FLG);
	if (*semid == -1)
	    attach_err(semkey, "semget");
	semctl(*semid, 0, SETVAL, s);
    }
}

void
sem_lock(int op, int semid)
{
    struct sembuf   sops;

    sops.sem_num = 0;
    sops.sem_flg = SEM_UNDO;
    sops.sem_op = op;
    if (semop(semid, &sops, 1)) {
	perror("semop");
	exit(1);
    }
}

/*
 * section - user cache(including uhash)
 */
/* uhash ****************************************** */
/*
 * the design is this: we use another stand-alone program to create and load
 * data into the hash. (that program could be run in rc-scripts or something
 * like that) after loading completes, the stand-alone program sets loaded to
 * 1 and exits.
 * 
 * the bbs exits if it can't attach to the shared memory or the hash is not
 * loaded yet.
 */

void
add_to_uhash(int n, const char *id)
{
    int            *p, h = StringHash(id)%(1<<HASH_BITS);
    int             times;
    strlcpy(SHM->userid[n], id, sizeof(SHM->userid[n]));

    p = &(SHM->hash_head[h]);

    for (times = 0; times < MAX_USERS && *p != -1; ++times)
	p = &(SHM->next_in_hash[*p]);

    if (times == MAX_USERS)
	abort_bbs(0);

    SHM->next_in_hash[*p = n] = -1;
}

void
remove_from_uhash(int n)
{
/*
 * note: after remove_from_uhash(), you should add_to_uhash() (likely with a
 * different name)
 */
    int             h = StringHash(SHM->userid[n])%(1<<HASH_BITS);
    int            *p = &(SHM->hash_head[h]);
    int             times;

    for (times = 0; times < MAX_USERS && (*p != -1 && *p != n); ++times)
	p = &(SHM->next_in_hash[*p]);

    if (times == MAX_USERS)
	abort_bbs(0);

    if (*p == n)
	*p = SHM->next_in_hash[n];
}

#if (1<<HASH_BITS)*10 < MAX_USERS
#warning "Suggest to use bigger HASH_BITS for better searchuser() performance,"
#warning "searchuser() average chaining MAX_USERS/(1<<HASH_BITS) times."
#endif
int
dosearchuser(const char *userid, char *rightid)
{
    int             h, p, times;
    STATINC(STAT_SEARCHUSER);
    h = StringHash(userid)%(1<<HASH_BITS);
    p = SHM->hash_head[h];

    for (times = 0; times < MAX_USERS && p != -1 && p < MAX_USERS ; ++times) {
	if (strcasecmp(SHM->userid[p], userid) == 0) {
	    if(userid[0] && rightid) strcpy(rightid, SHM->userid[p]);
	    return p + 1;
	}
	p = SHM->next_in_hash[p];
    }

    return 0;
}

int
searchuser(const char *userid, char *rightid)
{
    if(userid[0]=='\0')
	return 0;
    return dosearchuser(userid, rightid);
}

int
getuser(const char *userid, userec_t *xuser)
{
    int             uid;

    if ((uid = searchuser(userid, NULL))) {
	passwd_query(uid, xuser);
	xuser->money = moneyof(uid);
    }
    return uid;
}

char           *
getuserid(int num)
{
    if (--num >= 0 && num < MAX_USERS)
	return ((char *)SHM->userid[num]);
    return NULL;
}

void
setuserid(int num, const char *userid)
{
    if (num > 0 && num <= MAX_USERS) {
/*  Ptt: it may cause problems
	if (num > SHM->number)
	    SHM->number = num;
	else
*/
        remove_from_uhash(num - 1);
	add_to_uhash(num - 1, userid);
    }
}

#ifndef _BBS_UTIL_C_
char           *
u_namearray(char buf[][IDLEN + 1], int *pnum, char *tag)
{
    register char  *ptr, tmp;
    register int    n, total;
    char            tagbuf[STRLEN];
    int             ch, ch2, num;

    if (*tag == '\0') {
	*pnum = SHM->number;
	return SHM->userid[0];
    }
    for (n = 0; tag[n]; n++)
	tagbuf[n] = chartoupper(tag[n]);
    tagbuf[n] = '\0';
    ch = tagbuf[0];
    ch2 = ch - 'A' + 'a';
    total = SHM->number;
    for (n = num = 0; n < total; n++) {
	ptr = SHM->userid[n];
	tmp = *ptr;
	if (tmp == ch || tmp == ch2) {
	    if (chkstr(tag, tagbuf, ptr))
		strcpy(buf[num++], ptr);
	}
    }
    *pnum = num;
    return buf[0];
}
#endif

void
getnewutmpent(const userinfo_t * up)
{
/* Ptt:這裡加上 hash 觀念找空的 utmp */
    register int    i;
    register userinfo_t *uentp;
    unsigned int p = StringHash(up->userid) % USHM_SIZE;
    for (i = 0; i < USHM_SIZE; i++, p++) {
	if (p == USHM_SIZE)
	    p = 0;
	uentp = &(SHM->uinfo[p]);
	if (!(uentp->pid)) {
	    memcpy(uentp, up, sizeof(userinfo_t));
	    currutmp = uentp;
	    return;
	}
    }
    exit(1);
}

int
apply_ulist(int (*fptr) (const userinfo_t *))
{
    register userinfo_t *uentp;
    register int    i, state;

    for (i = 0; i < USHM_SIZE; i++) {
	uentp = &(SHM->uinfo[i]);
	if (uentp->pid && (PERM_HIDE(currutmp) || !PERM_HIDE(uentp)))
	    if ((state = (*fptr) (uentp)))
		return state;
    }
    return 0;
}

userinfo_t     *
search_ulist_pid(int pid)
{
    register int    i = 0, j, start = 0, end = SHM->UTMPnumber - 1;
    int *ulist;
    register userinfo_t *u;
    if (end == -1)
	return NULL;
    ulist = SHM->sorted[SHM->currsorted][8];
    for (i = ((start + end) / 2);; i = (start + end) / 2) {
	u = &SHM->uinfo[ulist[i]];
	j = pid - u->pid;
	if (!j) {
	    return u;
	}
	if (end == start) {
	    break;
	} else if (i == start) {
	    i = end;
	    start = end;
	} else if (j > 0)
	    start = i;
	else
	    end = i;
    }
    return 0;
}

userinfo_t     *
search_ulistn(int uid, int unum)
{
    register int    i = 0, j, start = 0, end = SHM->UTMPnumber - 1;
    int *ulist;
    register userinfo_t *u;
    if (end == -1)
	return NULL;
    ulist = SHM->sorted[SHM->currsorted][7];
    for (i = ((start + end) / 2);; i = (start + end) / 2) {
	u = &SHM->uinfo[ulist[i]];
	j = uid - u->uid;
	if (j == 0) {
	    for (; i > 0 && uid == SHM->uinfo[ulist[i - 1]].uid; --i)
		;/* 指到第一筆 */
	    // piaip Tue Jan  8 09:28:03 CST 2008
	    // many people bugged about that their utmp have invalid
	    // entry on record.
	    // we found them caused by crash process (DEBUGSLEEPING) which
	    // may occupy utmp entries even after process was killed.
	    // because the memory is invalid, it is not safe for those process
	    // to wipe their utmp entry. it should be done by some external
	    // daemon.
	    // however, let's make a little workaround here...
	    for (; unum > 0 && i >= 0 && ulist[i] >= 0 &&
		    SHM->uinfo[ulist[i]].uid == uid; unum--, i++)
	    {
		if (SHM->uinfo[ulist[i]].mode == DEBUGSLEEPING)
		    unum ++;
	    }
	    if (unum == 0 && i > 0 && ulist[i-1] >= 0 &&
		    SHM->uinfo[ulist[i-1]].uid == uid)
		return &SHM->uinfo[ulist[i-1]];
	    /*
	    if ( i + unum - 1 >= 0 &&
		 (ulist[i + unum - 1] >= 0 &&
		  uid == SHM->uinfo[ulist[i + unum - 1]].uid ) )
		return &SHM->uinfo[ulist[i + unum - 1]];
		*/
	    break;		/* 超過範圍 */
	}
	if (end == start) {
	    break;
	} else if (i == start) {
	    i = end;
	    start = end;
	} else if (j > 0)
	    start = i;
	else
	    end = i;
    }
    return 0;
}

userinfo_t     *
search_ulist_userid(const char *userid)
{
    register int    i = 0, j, start = 0, end = SHM->UTMPnumber - 1;
    int *ulist;
    register userinfo_t * u;
    if (end == -1)
	return NULL;
    ulist = SHM->sorted[SHM->currsorted][0];
    for (i = ((start + end) / 2);; i = (start + end) / 2) {
	u = &SHM->uinfo[ulist[i]];
	j = strcasecmp(userid, u->userid);
	if (!j) {
	    return u;
	}
	if (end == start) {
	    break;
	} else if (i == start) {
	    i = end;
	    start = end;
	} else if (j > 0)
	    start = i;
	else
	    end = i;
    }
    return 0;
}

#ifndef _BBS_UTIL_C_
int
count_logins(int uid, int show)
{
    register int    i = 0, j, start = 0, end = SHM->UTMPnumber - 1, count;
    int *ulist;
    userinfo_t *u; 
    if (end == -1)
	return 0;
    ulist = SHM->sorted[SHM->currsorted][7];
    for (i = ((start + end) / 2);; i = (start + end) / 2) {
	u = &SHM->uinfo[ulist[i]];
	j = uid - u->uid;
	if (!j) {
	    for (; i > 0 && uid == SHM->uinfo[ulist[i - 1]].uid; i--);
							/* 指到第一筆 */
	    for (count = 0; (ulist[i + count] &&
		    (u = &SHM->uinfo[ulist[i + count]]) &&
		    uid == u->uid); count++) {
		if (show)
		    prints("(%d) 目前狀態為: %-17.16s(來自 %s)\n",
			   count + 1, modestring(u, 0),
			   u->from);
	    }
	    return count;
	}
	if (end == start) {
	    break;
	} else if (i == start) {
	    i = end;
	    start = end;
	} else if (j > 0)
	    start = i;
	else
	    end = i;
    }
    return 0;
}

void
purge_utmp(userinfo_t * uentp)
{
    logout_friend_online(uentp);
    memset(uentp, 0, sizeof(userinfo_t));
    SHM->UTMPneedsort = 1;
}
#endif

/*
 * section - money cache
 */
int
setumoney(int uid, int money)
{
    SHM->money[uid - 1] = money;
    passwd_update_money(uid);
    return SHM->money[uid - 1];
}

int
deumoney(int uid, int money)
{
    if (uid <= 0 || uid > MAX_USERS){
#if defined(_BBS_UTIL_C_)
	printf("internal error: deumoney(%d, %d)\n", uid, money);
#else
	vmsg("internal error");
#endif
	return -1;
    }

    if (money < 0 && moneyof(uid) < -money)
	return setumoney(uid, 0);
    else
	return setumoney(uid, SHM->money[uid - 1] + money);
}

/*
 * section - utmp
 */
#if !defined(_BBS_UTIL_C_) /* _BBS_UTIL_C_ 不會有 utmp */
void
setutmpmode(unsigned int mode)
{
    if (currstat != mode)
	currutmp->mode = currstat = mode;
    /* 追蹤使用者 */
    if (HasUserPerm(PERM_LOGUSER)) {
	log_user("setutmpmode to %s(%d)\n", modestring(currutmp, 0), mode);
    }
}

unsigned int 
getutmpmode(void)
{
    if (currutmp)
	return currutmp->mode;
    return currstat;
}
#endif

/*
 * section - board cache
 */
void touchbtotal(int bid) {
    assert(0<=bid-1 && bid-1<MAX_BOARD);
    SHM->total[bid - 1] = 0;
    SHM->lastposttime[bid - 1] = 0;
}


/**
 * qsort comparison function - 照板名排序
 */
static int
cmpboardname(const void * i, const void * j)
{
    return strcasecmp(bcache[*(int*)i].brdname, bcache[*(int*)j].brdname);
}

/**
 * qsort comparison function - 先照群組排序、同一個群組內依板名排
 */
static int
cmpboardclass(const void * i, const void * j)
{
    boardheader_t *brd1 = &bcache[*(int*)i], *brd2 = &bcache[*(int*)j];
    int cmp;

    cmp=strncmp(brd1->title, brd2->title, 4);
    if(cmp!=0) return cmp;
    return strcasecmp(brd1->brdname, brd2->brdname);
}


void
sort_bcache(void)
{
    int             i;
    /* critical section 盡量不要呼叫  */
    /* 只有新增 或移除看板 需要呼叫到 */
    if(SHM->Bbusystate) {
	sleep(1);
	return;
    }
    SHM->Bbusystate = 1;
    for (i = 0; i < SHM->Bnumber; i++) {
	SHM->bsorted[0][i] = SHM->bsorted[1][i] = i;
    }
    qsort(SHM->bsorted[0], SHM->Bnumber, sizeof(int), cmpboardname);
    qsort(SHM->bsorted[1], SHM->Bnumber, sizeof(int), cmpboardclass);

    for (i = 0; i < SHM->Bnumber; i++) {
	bcache[i].firstchild[0] = 0;
	bcache[i].firstchild[1] = 0;
    }
    SHM->Bbusystate = 0;
}

#ifdef _BBS_UTIL_C_
void
reload_bcache(void)
{
    int     i, fd;
    pid_t   pid;
    for( i = 0 ; i < 10 && SHM->Bbusystate ; ++i ){
	printf("SHM->Bbusystate is currently locked (value: %d). "
	       "please wait... ", SHM->Bbusystate);
	sleep(1);
    }

    SHM->Bbusystate = 1;
    if ((fd = open(fn_board, O_RDONLY)) > 0) {
	SHM->Bnumber =
	    read(fd, bcache, MAX_BOARD * sizeof(boardheader_t)) /
	    sizeof(boardheader_t);
	close(fd);
    }
    memset(SHM->lastposttime, 0, MAX_BOARD * sizeof(time4_t));
    memset(SHM->total, 0, MAX_BOARD * sizeof(int));

    /* 等所有 boards 資料更新後再設定 uptime */
    SHM->Buptime = SHM->Btouchtime;
    log_usies("CACHE", "reload bcache");
    SHM->Bbusystate = 0;
    sort_bcache();

    printf("load bottom in background");
    if( (pid = fork()) > 0 )
	return;
    setproctitle("loading bottom");
    for( i = 0 ; i < MAX_BOARD ; ++i )
	if( SHM->bcache[i].brdname[0] ){
	    char    fn[128];
	    int n;
	    sprintf(fn, "boards/%c/%s/" FN_DIR ".bottom", 
		    SHM->bcache[i].brdname[0],
		    SHM->bcache[i].brdname);
	    n = get_num_records(fn, sizeof(fileheader_t));
	    if( n > 5 )
		n = 5;
	    SHM->n_bottom[i] = n;
	}
    printf("load bottom done");
    if( pid == 0 )
	exit(0);
    // if pid == -1 should be returned
}

void resolve_boards(void)
{
    while (SHM->Buptime < SHM->Btouchtime) {
	reload_bcache();
    }
    numboards = SHM->Bnumber;
}
#endif /* defined(_BBS_UTIL_C_)*/

#if 0
/* Unused */
void touch_boards(void)
{
    SHM->Btouchtime = COMMON_TIME;
    numboards = -1;
    resolve_boards();
}
#endif

void addbrd_touchcache(void)
{
    SHM->Bnumber++;
    numboards = SHM->Bnumber;
    reset_board(numboards);
    sort_bcache();
}

void
reset_board(int bid) /* XXXbid: from 1 */
{				/* Ptt: 這樣就不用老是touch board了 */
    int             fd;
    boardheader_t  *bhdr;

    if (--bid < 0)
	return;
    assert(0<=bid && bid<MAX_BOARD);
    if (SHM->Bbusystate || COMMON_TIME - SHM->busystate_b[bid] < 10) {
	safe_sleep(1);
    } else {
	SHM->busystate_b[bid] = COMMON_TIME;

	bhdr = bcache;
	bhdr += bid;
	if ((fd = open(fn_board, O_RDONLY)) > 0) {
	    lseek(fd, (off_t) (bid * sizeof(boardheader_t)), SEEK_SET);
	    read(fd, bhdr, sizeof(boardheader_t));
	    close(fd);
	}
	SHM->busystate_b[bid] = 0;

	buildBMcache(bid + 1); /* XXXbid */
    }
}

#ifndef _BBS_UTIL_C_ /* because of HasBoardPerm() in board.c */
int
apply_boards(int (*func) (boardheader_t *))
{
    register int    i;
    register boardheader_t *bhdr;

    for (i = 0, bhdr = bcache; i < numboards; i++, bhdr++) {
	if (!(bhdr->brdattr & BRD_GROUPBOARD) && HasBoardPerm(bhdr) &&
	    (*func) (bhdr) == QUIT)
	    return QUIT;
    }
    return 0;
}
#endif

void
setbottomtotal(int bid)
{
    boardheader_t  *bh = getbcache(bid);
    char            fname[PATHLEN];
    int             n;

    assert(0<=bid-1 && bid-1<MAX_BOARD);
    if(!bh->brdname[0]) return;
    setbfile(fname, bh->brdname, FN_DIR ".bottom");
    n = get_num_records(fname, sizeof(fileheader_t));
    if(n>5)
      {
#ifdef DEBUG_BOTTOM
        log_file("fix_bottom", LOG_CREAT | LOG_VF, "%s n:%d\n", fname, n);
#endif
        unlink(fname);
        SHM->n_bottom[bid-1]=0;
      }
    else
        SHM->n_bottom[bid-1]=n;
}
void
setbtotal(int bid)
{
    boardheader_t  *bh = getbcache(bid);
    struct stat     st;
    char            genbuf[256];
    int             num, fd;

    assert(0<=bid-1 && bid-1<MAX_BOARD);
    setbfile(genbuf, bh->brdname, FN_DIR);
    if ((fd = open(genbuf, O_RDWR)) < 0)
	return;			/* .DIR掛了 */
    fstat(fd, &st);
    num = st.st_size / sizeof(fileheader_t);
    assert(0<=bid-1 && bid-1<MAX_BOARD);
    SHM->total[bid - 1] = num;

    if (num > 0) {
	lseek(fd, (off_t) (num - 1) * sizeof(fileheader_t), SEEK_SET);
	if (read(fd, genbuf, FNLEN) >= 0) {
	    SHM->lastposttime[bid - 1] = (time4_t) atoi(&genbuf[2]);
	}
    } else
	SHM->lastposttime[bid - 1] = 0;
    close(fd);
}

void
touchbpostnum(int bid, int delta)
{
    int            *total = &SHM->total[bid - 1];
    assert(0<=bid-1 && bid-1<MAX_BOARD);
    if (*total)
	*total += delta;
}

int
getbnum(const char *bname)
{
    register int    i = 0, j, start = 0, end = SHM->Bnumber - 1;
    int *blist = SHM->bsorted[0];
    if(SHM->Bbusystate)
	sleep(1);
    for (i = ((start + end) / 2);; i = (start + end) / 2) {
	if (!(j = strcasecmp(bname, bcache[blist[i]].brdname)))
	    return (int)(blist[i] + 1);
	if (end == start) {
	    break;
	} else if (i == start) {
	    i = end;
	    start = end;
	} else if (j > 0)
	    start = i;
	else
	    end = i;
    }
    return 0;
}

const char *
postperm_msg(const char *bname)
{
    register int    i;
    char            buf[PATHLEN];
    boardheader_t   *bp = NULL;

    setbfile(buf, bname, fn_water);
    if (belong(buf, cuser.userid))
	return "使用者水桶中";

    if (!strcasecmp(bname, DEFAULT_BOARD))
	return NULL;

    if (!(i = getbnum(bname)))
	return "看板不存在";

    assert(0<=i-1 && i-1<MAX_BOARD);
    bp = getbcache(i);

    if (bp->brdattr & BRD_GUESTPOST)
        return NULL;

    if (!HasUserPerm(PERM_POST))
	return "無發文權限";

    /* 秘密看板特別處理 */
    if (bp->brdattr & BRD_HIDE)
	return NULL;
    else if (bp->brdattr & BRD_RESTRICTEDPOST &&
	    !is_hidden_board_friend(i, usernum))
	return "看板限制發文";

    if (HasUserPerm(PERM_VIOLATELAW) && (bp->level & PERM_VIOLATELAW))
	return NULL;
    else if (HasUserPerm(PERM_VIOLATELAW))
	return "罰單未繳";

    if (!(bp->level & ~PERM_POST))
	return NULL;
    if (!HasUserPerm(bp->level & ~PERM_POST))
	return "未達看板要求權限";
    return NULL;
}

int
haspostperm(const char *bname)
{
    return postperm_msg(bname) == NULL ? 1 : 0;
}

void buildBMcache(int bid) /* bid starts from 1 */
{
    char    s[IDLEN * 3 + 3], *ptr;
    int     i, uid;
    char   *strtok_pos;

    assert(0<=bid-1 && bid-1<MAX_BOARD);
    strlcpy(s, getbcache(bid)->BM, sizeof(s));
    for( i = 0 ; s[i] != 0 ; ++i )
	if( !isalpha((int)s[i]) && !isdigit((int)s[i]) )
            s[i] = ' ';

    for( ptr = strtok_r(s, " ", &strtok_pos), i = 0 ;
	 i < MAX_BMs && ptr != NULL  ;
	 ptr = strtok_r(NULL, " ", &strtok_pos), ++i  )
	if( (uid = searchuser(ptr, NULL)) != 0 )
	    SHM->BMcache[bid-1][i] = uid;
    for( ; i < MAX_BMs ; ++i )
	SHM->BMcache[bid-1][i] = -1;
}

int is_BM_cache(int bid) /* bid starts from 1 */
{
    assert(0<=bid-1 && bid-1<MAX_BOARD);
    --bid;
    // XXX hard coded MAX_BMs=4
    if( currutmp->uid == SHM->BMcache[bid][0] ||
	currutmp->uid == SHM->BMcache[bid][1] ||
	currutmp->uid == SHM->BMcache[bid][2] ||
	currutmp->uid == SHM->BMcache[bid][3]    ){
	cuser.userlevel |= PERM_BM;
	return 1;
    }
    return 0;
}

/*-------------------------------------------------------*/
/* PTT  cache                                            */
/*-------------------------------------------------------*/
int 
filter_aggressive(const char*s)
{
    if (
	/*
	strstr(s, "此處放較不適當的爭議性字句") != NULL ||
	*/
	0
	)
	return 1;
    return 0;
}

int 
filter_dirtywords(const char*s)
{
    if (
	strstr(s, "幹你娘") != NULL ||
	0)
	return 1;
    return 0;
}

#define AGGRESSIVE_FN ".aggressive"
static char drop_aggressive = 0;

void 
load_aggressive_state()
{
    if (dashf(AGGRESSIVE_FN))
	drop_aggressive = 1;
    else
	drop_aggressive = 0;
}

void 
set_aggressive_state(int s)
{
    FILE *fp = NULL;
    if (s)
    {
	fp = fopen(AGGRESSIVE_FN, "wb");
	fclose(fp);
    } else {
	remove(AGGRESSIVE_FN);
    }
}

/* cache for 動態看板 */
void
reload_pttcache(void)
{
    if (SHM->Pbusystate)
	safe_sleep(1);
    else {			/* jochang: temporary workaround */
	fileheader_t    item, subitem;
	char            pbuf[256], buf[256], *chr;
	FILE           *fp, *fp1, *fp2;
	int             id, aggid, rawid;

	SHM->Pbusystate = 1;
	SHM->last_film = 0;
	bzero(SHM->notes, sizeof(SHM->notes));
	setapath(pbuf, GLOBAL_NOTE);
	setadir(buf, pbuf);

	load_aggressive_state();
	id = aggid = rawid = 0; // effective count, aggressive count, total (raw) count

	if ((fp = fopen(buf, "r"))) {
	    // .DIR loop
	    while (fread(&item, sizeof(item), 1, fp)) {

		int chkagg = 0; // should we check aggressive?

		if (item.title[3] != '<' || item.title[8] != '>')
		    continue;

#ifdef GLOBAL_NOTE_AGGCHKDIR
		// TODO aggressive: only count '<點歌>' section
		if (strcmp(item.title+3, GLOBAL_NOTE_AGGCHKDIR) == 0)
		    chkagg = 1;
#endif

		snprintf(buf, sizeof(buf), "%s/%s/" FN_DIR,
			pbuf, item.filename);

		if (!(fp1 = fopen(buf, "r")))
		    continue;

		// file loop
		while (fread(&subitem, sizeof(subitem), 1, fp1)) {

		    snprintf(buf, sizeof(buf),
			    "%s/%s/%s", pbuf, item.filename,
			    subitem.filename);

		    if (!(fp2 = fopen(buf, "r")))
			continue;

		    fread(SHM->notes[id], sizeof(char), sizeof(SHM->notes[0]), fp2);
		    SHM->notes[id][sizeof(SHM->notes[0]) - 1] = 0;
		    rawid ++;

		    // filtering
		    if (filter_dirtywords(SHM->notes[id]))
		    {
			memset(SHM->notes[id], 0, sizeof(SHM->notes[0]));
			rawid --;
		    }
		    else if (chkagg && filter_aggressive(SHM->notes[id]))
		    {
			aggid++;
			// handle aggressive notes by last detemined state
			if (drop_aggressive)
			    memset(SHM->notes[id], 0, sizeof(SHM->notes[0]));
			else
			    id++;
#ifdef _BBS_UTIL_C_
			// Debug purpose
			// printf("found aggressive: %s\n", buf);
#endif
		    } 
		    else 
		    {
			id++;
		    }

		    fclose(fp2);
		    if (id >= MAX_MOVIE)
			break;

		} // end of file loop
		fclose(fp1);

		if (id >= MAX_MOVIE)
		    break;
	    } // end of .DIR loop
	    fclose(fp);

	    // decide next aggressive state
	    if (rawid && aggid*3 >= rawid) // if aggressive exceed 1/3
		set_aggressive_state(1);
	    else
		set_aggressive_state(0);

#ifdef _BBS_UTIL_C_
	    printf("id(%d)/agg(%d)/raw(%d)\n",
		    id, aggid, rawid);
#endif
	}
	SHM->last_film = id - 1;

	fp = fopen("etc/today_is", "r");
	if (fp) {
	    fgets(SHM->today_is, 15, fp);
	    if ((chr = strchr(SHM->today_is, '\n')))
		*chr = 0;
	    SHM->today_is[15] = 0;
	    fclose(fp);
	}
	/* 等所有資料更新後再設定 uptime */

	SHM->Puptime = SHM->Ptouchtime;
	log_usies("CACHE", "reload pttcache");
	SHM->Pbusystate = 0;
    }
}

void
resolve_garbage(void)
{
    int             count = 0;

    while (SHM->Puptime < SHM->Ptouchtime) {	/* 不用while等 */
	reload_pttcache();
	if (count++ > 10 && SHM->Pbusystate) {
	    /*
	     * Ptt: 這邊會有問題  load超過10 秒會所有進loop的process tate = 0
	     * 這樣會所有prcosee都會在load 動態看板 會造成load大增
	     * 但沒有用這個function的話 萬一load passwd檔的process死了
	     * 又沒有人把他 解開  同樣的問題發生在reload passwd
	     */
	    SHM->Pbusystate = 0;
#ifndef _BBS_UTIL_C_
	    log_usies("CACHE", "refork Ptt dead lock");
#endif
	}
    }
}

/*-------------------------------------------------------*/
/* PTT's cache                                           */
/*-------------------------------------------------------*/
/* cache for from host 與最多上線人數 */
void
reload_fcache(void)
{
    if (SHM->Fbusystate)
	safe_sleep(1);
    else {
	FILE           *fp;

	SHM->Fbusystate = 1;
	bzero(SHM->home_ip, sizeof(SHM->home_ip));
	if ((fp = fopen("etc/domain_name_query.cidr", "r"))) {
	    char            buf[256], *ip, *mask;
	    char *strtok_pos;

	    SHM->home_num = 0;
	    while (fgets(buf, sizeof(buf), fp)) {
		if (!buf[0] || buf[0] == '#' || buf[0] == ' ' || buf[0] == '\n')
		    continue;

		if (buf[0] == '@') {
		    SHM->home_ip[0] = 0;
		    SHM->home_mask[0] = 0xFFFFFFFF;
		    SHM->home_num++;
		    continue;
		}

		ip = strtok_r(buf, " \t", &strtok_pos);
		if ((mask = strchr(ip, '/')) != NULL) {
		    int shift = 32 - atoi(mask + 1);
		    SHM->home_ip[SHM->home_num] = ipstr2int(ip);
		    SHM->home_mask[SHM->home_num] = (0xFFFFFFFF >> shift ) << shift;
		}
		else {
		    SHM->home_ip[SHM->home_num] = ipstr2int(ip);
		    SHM->home_mask[SHM->home_num] = 0xFFFFFFFF;
		}
		ip = strtok_r(NULL, " \t", &strtok_pos);
		if (ip == NULL) {
		    strcpy(SHM->home_desc[SHM->home_num], "雲深不知處");
		}
		else {
		    strlcpy(SHM->home_desc[SHM->home_num], ip,
			    sizeof(SHM->home_desc[SHM->home_num]));
		    chomp(SHM->home_desc[SHM->home_num]);
		}
		(SHM->home_num)++;
		if (SHM->home_num == MAX_FROM)
		    break;
	    }
	    fclose(fp);
	}
	SHM->max_user = 0;

	/* 等所有資料更新後再設定 uptime */
	SHM->Fuptime = SHM->Ftouchtime;
#if !defined(_BBS_UTIL_C_)
	log_usies("CACHE", "reload fcache");
#endif
	SHM->Fbusystate = 0;
    }
}

void
resolve_fcache(void)
{
    while (SHM->Fuptime < SHM->Ftouchtime)
	reload_fcache();
}

/*
 * section - hbfl (hidden board friend list)
 */
void
hbflreload(int bid)
{
    int             hbfl[MAX_FRIEND + 1], i, num, uid;
    char            buf[128];
    FILE           *fp;

    assert(0<=bid-1 && bid-1<MAX_BOARD);
    memset(hbfl, 0, sizeof(hbfl));
    setbfile(buf, bcache[bid - 1].brdname, fn_visable);
    if ((fp = fopen(buf, "r")) != NULL) {
	for (num = 1; num <= MAX_FRIEND; ++num) {
	    if (fgets(buf, sizeof(buf), fp) == NULL)
		break;
	    for (i = 0; buf[i] != 0; ++i)
		if (buf[i] == ' ') {
		    buf[i] = 0;
		    break;
		}
	    if (strcasecmp(STR_GUEST, buf) == 0 ||
		(uid = searchuser(buf, NULL)) == 0) {
		--num;
		continue;
	    }
	    hbfl[num] = uid;
	}
	fclose(fp);
    }
    hbfl[0] = COMMON_TIME;
    memcpy(SHM->hbfl[bid-1], hbfl, sizeof(hbfl));
}

/* 是否通過板友測試. 如果在板友名單中的話傳回 1, 否則為 0 */
int
is_hidden_board_friend(int bid, int uid)
{
    int             i;

    assert(0<=bid-1 && bid-1<MAX_BOARD);
    if (SHM->hbfl[bid-1][0] < login_start_time - HBFLexpire)
	hbflreload(bid);
    for (i = 1; SHM->hbfl[bid-1][i] != 0 && i <= MAX_FRIEND; ++i) {
	if (SHM->hbfl[bid-1][i] == uid)
	    return 1;
    }
    return 0;
}

#ifdef USE_COOLDOWN
void add_cooldowntime(int uid, int min)
{
    // Ptt: I will use the number below 15 seconds.
    time4_t base= now > SHM->cooldowntime[uid - 1]? 
                    now : SHM->cooldowntime[uid - 1];
    base += min*60;
    base &= 0xFFFFFFF0;

    SHM->cooldowntime[uid - 1] = base;
}
void add_posttimes(int uid, int times)
{
  if((SHM->cooldowntime[uid - 1] & 0xF) + times <0xF)
       SHM->cooldowntime[uid - 1] += times;
  else
       SHM->cooldowntime[uid - 1] |= 0xF;
}
#endif
