/* $Id: mail.c 3924 2008-02-17 03:25:17Z piaip $ */
#include "bbs.h"
static int      mailkeep = 0,		mailsum = 0;
static int      mailsumlimit = 0,	mailmaxkeep = 0;
static char     currmaildir[32];
static char     msg_cc[] = ANSI_COLOR(32) "[群組名單]" ANSI_RESET "\n";
static char     listfile[] = "list.0";

// check only 20 mails (one page) is enough.
// #define NEWMAIL_CHECK_RANGE (1)
// checking only 1 mail works more like brc style.
#define NEWMAIL_CHECK_RANGE (5)

enum SHOWMAIL_MODES {
    SHOWMAIL_NORM = 0,
    SHOWMAIL_SUM,
    SHOWMAIL_RANGE,
};
static int	showmail_mode = SHOWMAIL_NORM;

int
setforward(void)
{
    char            buf[80], ip[50] = "", yn[4];
    FILE           *fp;
    int flIdiotSent2Self = 0;
    int oidlen = strlen(cuser.userid);

    sethomepath(buf, cuser.userid);
    strcat(buf, "/.forward");
    if ((fp = fopen(buf, "r"))) {
	fscanf(fp, "%" toSTR(sizeof(ip)) "s", ip);
	fclose(fp);
    }
    getdata_buf(b_lines - 1, 0, "請輸入自動轉寄的Email: ",
		ip, sizeof(ip), DOECHO);

    /* anti idiots */
    if (strncasecmp(ip, cuser.userid, oidlen) == 0)
    {
	int addrlen = strlen(ip);
	if(	addrlen == oidlen ||
		(addrlen > oidlen && 
		 strcasecmp(ip + oidlen, str_mail_address) == 0))
	    flIdiotSent2Self = 1;
    }

    if (ip[0] && ip[0] != ' ' && !flIdiotSent2Self) {
	getdata(b_lines, 0, "確定開啟自動轉信功\能?(Y/n)", yn, sizeof(yn),
		LCECHO);
	if (yn[0] != 'n' && (fp = fopen(buf, "w"))) {
	    fputs(ip, fp);
	    fclose(fp);
	    vmsg("設定完成!");
	    return 0;
	}
    }
    unlink(buf);
    if(flIdiotSent2Self)
	vmsg("自動轉寄是不會設定給自己的，想取消用空白就可以了。");
    else
	vmsg("取消自動轉信!");
    return 0;
}

int
toggle_showmail_mode(void)
{
    showmail_mode ++;
    showmail_mode %= SHOWMAIL_RANGE;
    return FULLUPDATE;
}

int
built_mail_index(void)
{
    char            genbuf[128];

    move(b_lines - 4, 0);
    outs("本功\能只在信箱檔毀損時使用，" ANSI_COLOR(1;33) "無法" ANSI_RESET "救回被刪除的信件。\n"
	 "除非您清楚這個功\能的作用，否則" ANSI_COLOR(1;33) "請不要使用" ANSI_RESET "。\n"
	 "警告：任意的使用將導致" ANSI_COLOR(1;33) "不可預期的結果" ANSI_RESET "！\n");
    getdata(b_lines - 1, 0,
	    "確定重建信箱?(y/N)", genbuf, 3,
	    LCECHO);
    if (genbuf[0] != 'y')
	return 0;

    snprintf(genbuf, sizeof(genbuf),
	     BBSHOME "/bin/buildir " BBSHOME "/home/%c/%s > /dev/null",
	     cuser.userid[0], cuser.userid);
    mouts(b_lines - 1, 0, ANSI_COLOR(1;31) "已經處理完畢!! 諸多不便 敬請原諒~" ANSI_RESET);
    system(genbuf);
    pressanykey();
    return 0;
}

int
sendalert(const char *userid, int alert)
{
    userinfo_t     *uentp = NULL;
    int             n, tuid, i;

    if ((tuid = searchuser(userid, NULL)) == 0)
	return -1;

    n = count_logins(tuid, 0);
    for (i = 1; i <= n; i++)
	if ((uentp = (userinfo_t *) search_ulistn(tuid, i)))
	    uentp->alerts |= alert;
    return 0;
}

int
mail_muser(userec_t muser, const char *title, const char *filename)
{
    return mail_id(muser.userid, title, filename, cuser.userid);
}

int
mail_id(const char *id, const char *title, const char *src, const char *owner)
{
    fileheader_t    mhdr;
    char            dst[128], dirf[128];
    sethomepath(dst, id);
    if (stampfile(dst, &mhdr))
	return 0;
    strlcpy(mhdr.owner, owner, sizeof(mhdr.owner));
    strlcpy(mhdr.title, title, sizeof(mhdr.title));
    mhdr.filemode = 0;
    Copy(src, dst);

    sethomedir(dirf, id);
    append_record_forward(dirf, &mhdr, sizeof(mhdr), id);
    sendalert(id, ALERT_NEW_MAIL);
    return 0;
}

int
invalidaddr(const char *addr)
{
#ifdef DEBUG_FWDADDRERR
    const char *origaddr = addr;
    char errmsg[PATHLEN];
#endif

    if (*addr == '\0')
	return 1;		/* blank */

    while (*addr) {
#ifdef DEBUG_FWDADDRERR
	if (not_alnum(*addr) && !strchr("[].@-_+", *addr))
	{
	    int c = (*addr) & 0xff;
	    clear();
	    move(2,0);
	    outs(
		"您輸入的位址錯誤 (address error)。 \n\n"
		"由於最近許\多人反應打入正確的位址(id或email)後系統會判斷錯誤\n"
		"但檢查不出原因，所以我們需要正確的錯誤回報。\n\n"
		"如果你確實打錯了，請直接略過下面的說明。\n"
		"如果你認為你輸入的位址確實是對的，請把下面的訊息複製起來\n"
		"並貼到 " GLOBAL_BUGREPORT " 板。本站為造成不便深感抱歉。\n\n"
		ANSI_COLOR(1;33));
	    sprintf(errmsg, "原始輸入位址: [%s]\n"
		    "錯誤位置: 第 %d 字元: 0x%02X [ %c ]\n", 
		    origaddr, (int)(addr - origaddr+1), c, c);
	    outs(errmsg);
	    outs(ANSI_RESET);
	    vmsg("請按任意鍵繼續");
	    clear();
	    return 1;
	}
#else
	if (not_alnum(*addr) && !strchr("[].@-_", *addr))
	    return 1;
#endif
	addr++;
    }
    return 0;
}

int
m_internet(void)
{
    char            receiver[60];
    char title[STRLEN];

    getdata(20, 0, "收信人：", receiver, sizeof(receiver), DOECHO);
    trim(receiver);
    if (strchr(receiver, '@') && !invalidaddr(receiver) &&
	getdata(21, 0, "主  題：", title, sizeof(title), DOECHO))
	do_send(receiver, title);
    else {
	vmsg("收信人或主題不正確,請重新選取指令");
    }
    return 0;
}

void
m_init(void)
{
    sethomedir(currmaildir, cuser.userid);
}

static void
loadmailusage(void)
{
    mailkeep=get_num_records(currmaildir,sizeof(fileheader_t));
    mailsum =get_sum_records(currmaildir, sizeof(fileheader_t));
}

void
setupmailusage(void)
{  // Ptt: get_sum_records is a bad function
	int             max_keepmail = MAX_KEEPMAIL;
#ifdef PLAY_ANGEL
	if (HasUserPerm(PERM_SYSSUPERSUBOP | PERM_ANGEL))
#else
	if (HasUserPerm(PERM_SYSSUPERSUBOP))
#endif
	{
	    mailsumlimit = 900;
	    max_keepmail = 700;
	}
	else if (HasUserPerm(PERM_SYSSUBOP | PERM_ACCTREG | PERM_PRG |
		     PERM_ACTION | PERM_PAINT)) {
	    mailsumlimit = 700;
	    max_keepmail = 500;
	} else if (HasUserPerm(PERM_BM)) {
	    mailsumlimit = 500;
	    max_keepmail = 300;
	} else if (HasUserPerm(PERM_LOGINOK))
	    mailsumlimit = 200;
	else
	    mailsumlimit = 50;
	mailsumlimit += (cuser.exmailbox + ADD_EXMAILBOX) * 10;
	mailmaxkeep = max_keepmail + cuser.exmailbox;
	loadmailusage();
}

#define MAILBOX_LIM_OK   0
#define MAILBOX_LIM_KEEP 1
#define MAILBOX_LIM_SUM  2
static int
chk_mailbox_limit(void)
{
    if (HasUserPerm(PERM_SYSOP) || HasUserPerm(PERM_MAILLIMIT))
	return MAILBOX_LIM_OK;

    if (!mailkeep)
	setupmailusage();

    if (mailkeep > mailmaxkeep)
	return MAILBOX_LIM_KEEP;
    if (mailsum > mailsumlimit)
	return MAILBOX_LIM_SUM;
    return MAILBOX_LIM_OK;
}

int
chkmailbox(void)
{
    m_init();

    switch (chk_mailbox_limit()) {
	case MAILBOX_LIM_KEEP:
	    bell();
	    bell();
	    vmsgf("您保存信件數目 %d 超出上限 %d, 請整理", mailkeep, mailmaxkeep);
	    return mailkeep;

	case MAILBOX_LIM_SUM:
	    bell();
	    bell();
	    vmsgf("信箱容量(大小,非件數) %d 超出上限 %d, "
		"請砍過長的水球記錄或信件", mailsum, mailsumlimit);
	    if(showmail_mode != SHOWMAIL_SUM)
	    {
		showmail_mode = SHOWMAIL_SUM;
		vmsg("信箱顯示模式已自動改為顯示大小，請盡速整理");
	    }
	    return mailsum;

	default:
	    return 0;
    }
}

static void
do_hold_mail(const char *fpath, const char *receiver, const char *holder)
{
    char            buf[80], title[128];

    fileheader_t    mymail;

    sethomepath(buf, holder);
    stampfile(buf, &mymail);

    mymail.filemode = FILE_READ ;
    strlcpy(mymail.owner, "[備.忘.錄]", sizeof(mymail.owner));
    if (receiver) {
	snprintf(title, sizeof(title), "(%s) %s", receiver, save_title);
	strlcpy(mymail.title, title, sizeof(mymail.title));
    } else
	strlcpy(mymail.title, save_title, sizeof(mymail.title));

    sethomedir(title, holder);

    unlink(buf);
    Copy(fpath, buf);
    append_record_forward(title, &mymail, sizeof(mymail), holder);
}

void
hold_mail(const char *fpath, const char *receiver)
{
    char            buf[4];

    getdata(b_lines - 1, 0, "已順利寄出，是否自存底稿(Y/N)？[N] ",
	    buf, sizeof(buf), LCECHO);

    if (buf[0] == 'y')
	do_hold_mail(fpath, receiver, cuser.userid);
}

int
do_send(const char *userid, const char *title)
{
    fileheader_t    mhdr;
    char            fpath[STRLEN];
    char            receiver[IDLEN + 1];
    char            genbuf[200];
    int             internet_mail, i;
    userec_t        xuser;

    STATINC(STAT_DOSEND);
    if (strchr(userid, '@'))
	internet_mail = 1;
    else {
	internet_mail = 0;
	if (!getuser(userid, &xuser))
	    return -1;
	if (!(xuser.userlevel & PERM_READMAIL))
	    return -3;

	curredit |= EDIT_MAIL;
	curredit &= ~EDIT_ITEM;
    }
    /* process title */
    if (title)
	strlcpy(save_title, title, sizeof(save_title));
    else {
	char tmp_title[STRLEN-20];
	getdata(2, 0, "主題：", tmp_title, sizeof(tmp_title), DOECHO);
	strlcpy(save_title, tmp_title, sizeof(save_title));
    }

    setutmpmode(SMAIL);

    fpath[0] = '\0';

    if (internet_mail) {
	int             res, ch;

	if (vedit(fpath, NA, NULL) == -1) {
	    unlink(fpath);
	    clear();
	    return -2;
	}
	clear();
	prints("信件即將寄給 %s\n標題為：%s\n確定要寄出嗎? (Y/N) [Y]",
	       userid, save_title);
	ch = igetch();
	switch (ch) {
	case 'N':
	case 'n':
	    outs("N\n信件已取消");
	    res = -2;
	    break;
	default:
	    outs("Y\n請稍候, 信件傳遞中...\n");
	    res =
#ifndef USE_BSMTP
		bbs_sendmail(fpath, save_title, userid);
#else
		bsmtp(fpath, save_title, userid);
#endif
	    hold_mail(fpath, userid);
	}
	unlink(fpath);
	return res;
    } else {
	strlcpy(receiver, userid, sizeof(receiver));
	sethomepath(genbuf, userid);
	stampfile(genbuf, &mhdr);
	strlcpy(mhdr.owner, cuser.userid, sizeof(mhdr.owner));
	if (vedit(genbuf, YEA, NULL) == -1) {
	    unlink(genbuf);
	    clear();
	    return -2;
	}
	/* why not make title here? */
	strlcpy(mhdr.title, save_title, sizeof(mhdr.title));
	clear();
	sethomefile(fpath, userid, FN_OVERRIDES);
	i = belong(fpath, cuser.userid);
	sethomefile(fpath, userid, FN_REJECT);

	if (i || !belong(fpath, cuser.userid)) {/* Ptt: 用belong有點討厭 */
	    sethomedir(fpath, userid);
	    if (append_record_forward(fpath, &mhdr, sizeof(mhdr), userid) == -1)
		return -1;
	    sendalert(userid,ALERT_NEW_MAIL);
	}
	hold_mail(genbuf, userid);
	return 0;
    }
}

void
my_send(const char *uident)
{
    switch (do_send(uident, NULL)) {
	case -1:
	outs(err_uid);
	break;
    case -2:
	outs(msg_cancel);
	break;
    case -3:
	prints("使用者 [%s] 無法收信", uident);
	break;
    }
    pressanykey();
}

int
m_send(void)
{
    char            uident[40];

    stand_title("且聽風的話");
    usercomplete(msg_uid, uident);
    showplans(uident);
    if (uident[0])
	my_send(uident);
    return 0;
}

/* 群組寄信、回信 : multi_send, multi_reply */
static void
multi_list(int *reciper)
{
    char            uid[16];
    char            genbuf[200];

    while (1) {
	stand_title("群組寄信名單");
	ShowNameList(3, 0, msg_cc);
	move(1, 0);
	outs("(I)引入好友 (O)引入上線通知 (N)引入新文章通知 (0-9)引入其他特別名單");
	getdata(2, 0,
	       "(A)增加     (D)刪除         (M)確認寄信名單   (Q)取消 ？[M]",
		genbuf, 4, LCECHO);
	switch (genbuf[0]) {
	case 'a':
	    while (1) {
		move(1, 0);
		usercomplete("請輸入要增加的代號(只按 ENTER 結束新增): ", uid);
		if (uid[0] == '\0')
		    break;

		move(2, 0);
		clrtoeol();

		if (!searchuser(uid, uid))
		    outs(err_uid);
		else if (!InNameList(uid)) {
		    AddNameList(uid);
		    (*reciper)++;
		}
		ShowNameList(3, 0, msg_cc);
	    }
	    break;
	case 'd':
	    while (*reciper) {
		move(1, 0);
		namecomplete("請輸入要刪除的代號(只按 ENTER 結束刪除): ", uid);
		if (uid[0] == '\0')
		    break;
		if (RemoveNameList(uid))
		    (*reciper)--;
		ShowNameList(3, 0, msg_cc);
	    }
	    break;
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
	    listfile[5] = genbuf[0];
	    genbuf[0] = '1';
	case 'i':
	    setuserfile(genbuf, genbuf[0] == '1' ? listfile : fn_overrides);
	    ToggleNameList(reciper, genbuf, msg_cc);
	    break;
	case 'o':
	    setuserfile(genbuf, "alohaed");
	    ToggleNameList(reciper, genbuf, msg_cc);
	    break;
	case 'n':
	    setuserfile(genbuf, "postlist");
	    ToggleNameList(reciper, genbuf, msg_cc);
	    break;
	case 'q':
	    *reciper = 0;
	    return;
	default:
	    return;
	}
    }
}

static void
multi_send(char *title)
{
    FILE           *fp;
    struct word_t  *p = NULL;
    fileheader_t    mymail;
    char            fpath[TTLEN], *ptr;
    int             reciper, listing;
    char            genbuf[256];

    CreateNameList();
    listing = reciper = 0;
    if (*quote_file) {
	AddNameList(quote_user);
	reciper = 1;
	fp = fopen(quote_file, "r");
	assert(fp);
	while (fgets(genbuf, sizeof(genbuf), fp)) {
	    if (strncmp(genbuf, "※ ", 3)) {
		if (listing)
		    break;
	    } else {
		if (listing) {
		    char *strtok_pos;
		    ptr = genbuf + 3;
		    for (ptr = strtok_r(ptr, " \n\r", &strtok_pos);
			    ptr;
			    ptr = strtok_r(NULL, " \n\r", &strtok_pos)) {
			if (searchuser(ptr, ptr) && !InNameList(ptr) &&
			    strcmp(cuser.userid, ptr)) {
			    AddNameList(ptr);
			    reciper++;
			}
		    }
		} else if (!strncmp(genbuf + 3, "[通告]", 6))
		    listing = 1;
	    }
	}
	fclose(fp);
	ShowNameList(3, 0, msg_cc);
    }
    multi_list(&reciper);
    move(1, 0);
    clrtobot();

    if (reciper) {
	setutmpmode(SMAIL);
	if (title)
	    do_reply_title(2, title);
	else {
	    getdata(2, 0, "主題：", fpath, sizeof(fpath), DOECHO);
	    snprintf(save_title, sizeof(save_title), "[通告] %s", fpath);
	}

	setuserfile(fpath, fn_notes);

	if ((fp = fopen(fpath, "w"))) {
	    fprintf(fp, "※ [通告] 共 %d 人收件", reciper);
	    listing = 80;

	    for (p = toplev; p; p = p->next) {
		reciper = strlen(p->word) + 1;
		if (listing + reciper > 75) {
		    listing = reciper;
		    fprintf(fp, "\n※");
		} else
		    listing += reciper;

		fprintf(fp, " %s", p->word);
	    }
	    memset(genbuf, '-', 75);
	    genbuf[75] = '\0';
	    fprintf(fp, "\n%s\n\n", genbuf);
	    fclose(fp);
	}
	curredit |= EDIT_LIST;

	if (vedit(fpath, YEA, NULL) == -1) {
	    unlink(fpath);
	    curredit = 0;
	    vmsg(msg_cancel);
	    return;
	}
	listing = 80;

	for (p = toplev; p; p = p->next) {
	    reciper = strlen(p->word) + 1;
	    if (listing + reciper > 75) {
		listing = reciper;
		outc('\n');
	    } else {
		listing += reciper;
		outc(' ');
	    }
	    outs(p->word);
	    if (searchuser(p->word, p->word) && strcmp(STR_GUEST, p->word)) {
		sethomefile(genbuf, p->word, FN_OVERRIDES);
		if (!belong(genbuf, cuser.userid)) { // not friend, check if rejected
		    sethomefile(genbuf, p->word, FN_REJECT);
		    if (belong(genbuf, cuser.userid))
			continue;
		}
		sethomepath(genbuf, p->word);
	    } else
		continue;
	    stampfile(genbuf, &mymail);
	    unlink(genbuf);
	    Copy(fpath, genbuf);

	    strlcpy(mymail.owner, cuser.userid, sizeof(mymail.owner));
	    strlcpy(mymail.title, save_title, sizeof(mymail.title));
	    mymail.filemode |= FILE_MULTI;	/* multi-send flag */
	    sethomedir(genbuf, p->word);
	    if (append_record_forward(genbuf, &mymail, sizeof(mymail), p->word) == -1)
		vmsg(err_uid);
	    sendalert(p->word, ALERT_NEW_MAIL);
	}
	hold_mail(fpath, NULL);
	unlink(fpath);
	curredit = 0;
    } else
	vmsg(msg_cancel);
}

static int
multi_reply(int ent, fileheader_t * fhdr, const char *direct)
{
    if (!(fhdr->filemode & FILE_MULTI))
	return mail_reply(ent, fhdr, direct);

    stand_title("群組回信");
    strlcpy(quote_user, fhdr->owner, sizeof(quote_user));
    setuserfile(quote_file, fhdr->filename);
    multi_send(fhdr->title);
    quote_user[0]='\0';
    quote_file[0]='\0';
    return FULLUPDATE;
}

int
mail_list(void)
{
    stand_title("群組作業");
    multi_send(NULL);
    return 0;
}

int
mail_all(void)
{
    FILE           *fp;
    fileheader_t    mymail;
    char            fpath[TTLEN];
    char            genbuf[200];
    int             i, unum;
    char           *userid;

    stand_title("給所有使用者的系統通告");
    setutmpmode(SMAIL);
    getdata(2, 0, "主題：", fpath, sizeof(fpath), DOECHO);
    snprintf(save_title, sizeof(save_title),
	     "[系統通告]" ANSI_COLOR(1;32) " %s" ANSI_RESET, fpath);

    setuserfile(fpath, fn_notes);

    if ((fp = fopen(fpath, "w"))) {
	fprintf(fp, "※ [" ANSI_COLOR(1) "系統通告" ANSI_RESET "] 這是封給所有使用者的信\n");
	fprintf(fp, "-----------------------------------------------------"
		"----------------------\n");
	fclose(fp);
    }
    *quote_file = 0;

    curredit |= EDIT_MAIL;
    curredit &= ~EDIT_ITEM;
    if (vedit(fpath, YEA, NULL) == -1) {
	curredit = 0;
	unlink(fpath);
	outs(msg_cancel);
	pressanykey();
	return 0;
    }
    curredit = 0;

    setutmpmode(MAILALL);
    stand_title("寄信中...");

    sethomepath(genbuf, cuser.userid);
    stampfile(genbuf, &mymail);
    unlink(genbuf);
    Copy(fpath, genbuf);
    unlink(fpath);
    strcpy(fpath, genbuf);

    strlcpy(mymail.owner, cuser.userid, sizeof(mymail.owner));	/* 站長 ID */
    strlcpy(mymail.title, save_title, sizeof(mymail.title));

    sethomedir(genbuf, cuser.userid);
    if (append_record_forward(genbuf, &mymail, sizeof(mymail), cuser.userid) == -1)
	outs(err_uid);

    for (unum = SHM->number, i = 0; i < unum; i++) {
	if (bad_user_id(SHM->userid[i]))
	    continue;		/* Ptt */

	userid = SHM->userid[i];
	if (strcmp(userid, STR_GUEST) && strcmp(userid, "new") &&
	    strcmp(userid, cuser.userid)) {
	    sethomepath(genbuf, userid);
	    stampfile(genbuf, &mymail);
	    unlink(genbuf);
	    Copy(fpath, genbuf);

	    strlcpy(mymail.owner, cuser.userid, sizeof(mymail.owner));
	    strlcpy(mymail.title, save_title, sizeof(mymail.title));
	    /* mymail.filemode |= FILE_MARKED; Ptt 公告改成不會mark */
	    sethomedir(genbuf, userid);
	    if (append_record_forward(genbuf, &mymail, sizeof(mymail), userid) == -1)
		outs(err_uid);
	    vmsgf("%*s %5d / %5d", IDLEN + 1, userid, i + 1, unum);
	}
    }
    return 0;
}

int
mail_mbox(void)
{
    char            cmd[100];
    fileheader_t    fhdr;

    snprintf(cmd, sizeof(cmd), "/tmp/%s.uu", cuser.userid);
    snprintf(fhdr.title, sizeof(fhdr.title), "%s 私人資料", cuser.userid);
    doforward(cmd, &fhdr, 'Z');
    return 0;
}

static int
m_forward(int ent, fileheader_t * fhdr, const char *direct)
{
    char            uid[STRLEN];

    stand_title("轉達信件");
    usercomplete(msg_uid, uid);
    if (uid[0] == '\0')
	return FULLUPDATE;

    strlcpy(quote_user, fhdr->owner, sizeof(quote_user));
    setuserfile(quote_file, fhdr->filename);
    snprintf(save_title, sizeof(save_title), "%.64s (fwd)", fhdr->title);
    move(1, 0);
    clrtobot();
    prints("轉信給: %s\n標  題: %s\n", uid, save_title);

    switch (do_send(uid, save_title)) {
    case -1:
	outs(err_uid);
	break;
    case -2:
	outs(msg_cancel);
	break;
    case -3:
	prints("使用者 [%s] 無法收信", uid);
	break;
    }
    pressanykey();
    quote_user[0]='\0';
    quote_file[0]='\0';
    if (strcasecmp(uid, cuser.userid) == 0)
	return DIRCHANGED;
    return FULLUPDATE;
}

struct ReadNewMailArg {
    int idc;
    int *delmsgs;
    int delcnt;
    int mrd;
};

static int
read_new_mail(void * voidfptr, void *optarg)
{
    fileheader_t *fptr=(fileheader_t*)voidfptr;
    struct ReadNewMailArg *arg=(struct ReadNewMailArg*)optarg;
    char            done = NA, delete_it;
    char            fname[PATHLEN];
    char            genbuf[4];

    arg->idc++;
    // XXX fptr->filename may be invalid.
    if (fptr->filemode || !fptr->filename[0])
	return 0;
    clear();
    move(10, 0);
    prints("您要讀來自[%s]的訊息(%s)嗎？", fptr->owner, fptr->title);
    getdata(11, 0, "請您確定(Y/N/Q)?[Y] ", genbuf, 3, DOECHO);
    if (genbuf[0] == 'q')
	return QUIT;
    if (genbuf[0] == 'n')
	return 0;

    setuserfile(fname, fptr->filename);
    fptr->filemode |= FILE_READ;
    if (substitute_record(currmaildir, fptr, sizeof(*fptr), arg->idc))
	return -1;

    arg->mrd = 1;
    delete_it = NA;
    while (!done) {
	int more_result = more(fname, YEA);

        switch (more_result) {
        case 999:
	    return mail_reply(arg->idc, fptr, currmaildir);
        case -1:
            return READ_SKIP;
        case 0:
            break;
        default:
            return more_result;
        }

	outmsglr(MSG_MAILER, MSG_MAILER_LEN, "", 0);

	switch (igetch()) {
	case 'r':
	case 'R':
	    mail_reply(arg->idc, fptr, currmaildir);
	    break;
	case 'x':
	    m_forward(arg->idc, fptr, currmaildir);
	    break;
	case 'y':
	    multi_reply(arg->idc, fptr, currmaildir);
	    break;
	case 'd':
	case 'D':
	    delete_it = YEA;
	default:
	    done = YEA;
	}
    }
    if (delete_it) {
	if(arg->delcnt==1000) {
	    vmsg("一次最多刪 1000 封信");
	    return 0;
	}
	clear();
	prints("刪除信件《%s》", fptr->title);
	getdata(1, 0, msg_sure_ny, genbuf, 2, LCECHO);
	if (genbuf[0] == 'y') {
	    if(arg->delmsgs==NULL) {
		arg->delmsgs=(int*)malloc(sizeof(int)*1000);
		if(arg->delmsgs==NULL) {
		    vmsg("失敗, 請洽站長");
		    return 0;
		}
	    }
	    unlink(fname);
	    arg->delmsgs[arg->delcnt++] = arg->idc;

	    loadmailusage();
	}
    }
    clear();
    return 0;
}

void setmailalert()
{
    if(load_mailalert(cuser.userid))
           currutmp->alerts |= ALERT_NEW_MAIL;
    else
           currutmp->alerts &= ~ALERT_NEW_MAIL;
}
int
m_new(void)
{
    struct ReadNewMailArg arg;
    clear();
    setutmpmode(RMAIL);
    memset(&arg, 0, sizeof(arg));
    clear();
    curredit |= EDIT_MAIL;
    curredit &= ~EDIT_ITEM;
    if (apply_record(currmaildir, read_new_mail, sizeof(fileheader_t), &arg) == -1) {
	if(arg.delmsgs)
	    free(arg.delmsgs);
	vmsg("沒有新信件了");
	return -1;
    }
    curredit = 0;
    setmailalert();
    while (arg.delcnt--)
	delete_record(currmaildir, sizeof(fileheader_t), arg.delmsgs[arg.delcnt]);
    if(arg.delmsgs)
	free(arg.delmsgs);
    vmsg(arg.mrd ? "信已閱\畢" : "沒有新信件了");
    return -1;
}

static void
mailtitle(void)
{
    char buf[STRLEN];
    int  msglen = 0;

    showtitle("郵件選單", BBSName);
    prints("[←]離開[↑↓]選擇[→]閱\讀信件 [R]回信 [x]轉達 "
	     "[y]群組回信 [O]站外信:%s [h]求助\n" 
	     ANSI_COLOR(7) "  編號   %s 作 者          信  件  標  題" 
	     "",
	     REJECT_OUTTAMAIL ? ANSI_COLOR(31) "關" ANSI_RESET : "開",
	     (showmail_mode == SHOWMAIL_SUM) ? "大 小":"日 期");

    /* 43 columns in length, used later. */
    buf[0] = 0;

    if (mailsumlimit)
    {
	/* warning: snprintf returns length "if not limited".
	 * however if this case, they should be the same. */

	msglen = snprintf(buf, sizeof(buf),
		ANSI_COLOR(32) 
		 " (容量:%d/%dk %d/%d篇) ",
		 mailsum, mailsumlimit,
		 mailkeep, mailmaxkeep);
	msglen -= strlen(ANSI_COLOR(32));
    }
    outslr("", 44, buf, msglen);
    outs(ANSI_RESET);
}

static void
maildoent(int num, fileheader_t * ent)
{
    char *title, *mark, *color = NULL, type = ' ';
    char datepart[6];
    char isonline = 0;

    if (ent->filemode & FILE_MARKED)
    {
	type = (ent->filemode & FILE_READ) ?
	    'm' : 'M';
    } 
    else if (ent->filemode & FILE_REPLIED)
    {
	type = (ent->filemode & FILE_READ) ?
	    'r' : 'R';
    } 
    else 
    {
	type = (ent->filemode & FILE_READ) ?
	    ' ' : '+';
    }

    if (TagNum && !Tagger(atoi(ent->filename + 2), 0, TAG_NIN))
	type = 'D';

    title = subject(mark = ent->title);
    if (title == mark) {
	color = ANSI_COLOR(1;31);
	mark = "◇";
    } else {
	color = ANSI_COLOR(1;33);
	mark = "R:";
    }
    
    strlcpy(datepart, ent->date, sizeof(datepart));

    isonline = query_online(ent->owner);

    switch(showmail_mode)
    {
	case SHOWMAIL_SUM:
	    {
		/* evaluate size */
		size_t filesz = 0;
		char ut = 'k';
		char buf[MAXPATHLEN];
		struct stat st;

		if( !ent->filename[0] ){
		    filesz = 0;
		} else {
		    setuserfile(buf, ent->filename);
		    if (stat(buf, &st) >= 0) {
			filesz = st.st_size;
			/* find printing unit */
			filesz = (filesz + 1023) / 1024;
			if(filesz > 9999){
			    filesz = (filesz+512) / 1024; 
			    ut = 'M';
			}
			if(filesz > 9999) {
			    filesz = (filesz+512) / 1024;
			    ut = 'G';
			}
		    }
		}
		sprintf(datepart, "%4lu%c", (unsigned long)filesz, ut);
	    }
	    break;
	default:
	    break;
    }

    /* print out */
    if (strncmp(currtitle, title, TTLEN) != 0)
    {
	/* is title. */
	color = "";
    }

    prints("%6d %c %-6s%s%-15.14s%s%s %s%-*.*s%s\n", 
	    num, type, datepart, 
	    isonline ? ANSI_COLOR(1) : "",
	    ent->owner, 
	    isonline ? ANSI_RESET : "",
	    mark, color,
	    t_columns - 34, t_columns - 34,
	    title,
	    *color ? ANSI_RESET : "");
}


static int
mail_del(int ent, const fileheader_t * fhdr, const char *direct)
{
    char            genbuf[200];

    if (fhdr->filemode & FILE_MARKED)
	return DONOTHING;

    if (currmode & MODE_SELECT) {
        vmsg("請先回到正常模式後再進行刪除...");
        return READ_REDRAW;
    }

    if (getans(msg_del_ny) == 'y') {
	if (!delete_record(direct, sizeof(*fhdr), ent)) {
            setupmailusage();
	    setdirpath(genbuf, direct, fhdr->filename);
#ifdef USE_RECYCLE
	    RcyAddFile(fhdr, 0, genbuf);
#endif // USE_RECYCLE
	    unlink(genbuf);
	    loadmailusage();
	    return DIRCHANGED;
	}
    }
    return READ_REDRAW;
}

int b_call_in(int ent, const fileheader_t * fhdr, const char *direct);

static int
mail_read(int ent, fileheader_t * fhdr, const char *direct)
{
    char            buf[PATHLEN];
    char            done, delete_it, replied;

    clear();
    setdirpath(buf, direct, fhdr->filename);
    strlcpy(currtitle, subject(fhdr->title), sizeof(currtitle));
    done = delete_it = replied = NA;
    while (!done) {
	int             more_result = more(buf, YEA);

	/* whether success or not, update flag.
	 * or users may bug about "black-hole" mails
	 * and blinking notification */
	if( !(fhdr->filemode & FILE_READ))
	{
	    fhdr->filemode |= FILE_READ;
	    substitute_ref_record(direct, fhdr, ent);
	}
	switch (more_result) {
	case -1:
	    /* no such file */
	    clear();
	    vmsg("此封信無內容。");
	    return FULLUPDATE;
	case 999:
	    return mail_reply(ent, fhdr, direct);
        case 0:
            break;
	default:
            return more_result;
	}
	outmsglr(MSG_MAILER, MSG_MAILER_LEN, "", 0);

	switch (igetch()) {
	case 'r':
	case 'R':
	    replied = YEA;
	    mail_reply(ent, fhdr, direct);
	    break;
	case 'x':
	    m_forward(ent, fhdr, direct);
	    break;
	case 'y':
	    multi_reply(ent, fhdr, direct);
	    break;
	case 'd':
	    delete_it = YEA;
	default:
	    done = YEA;
	}
    }
    if (delete_it)
	mail_del(ent, fhdr, direct);
    else {
	fhdr->filemode |= FILE_READ;
        substitute_ref_record(direct, fhdr, ent);
    }
    return FULLUPDATE;
}

static int
mail_read_all(int ent, fileheader_t * fhdr, const char *direct)
{
    off_t   i = 0, num = 0;
    int	    fd = 0;
    fileheader_t xfhdr;

    currutmp->alerts &= ~ALERT_NEW_MAIL;
    if ((fd = open(currmaildir, O_RDWR)) < 0)
	return DONOTHING;

    if ((num = lseek(fd, 0, SEEK_END)) < 0)
	num = 0;
    num /= sizeof(fileheader_t);

    i = num - NEWMAIL_CHECK_RANGE;
    if (i < 0) i = 0;

    if (lseek(fd, i * (off_t)sizeof(fileheader_t), SEEK_SET) < 0)
	i = num;

    for (; i < num; i++)
    {
	if (read(fd, &xfhdr, sizeof(xfhdr)) <= 0)
	    break;
	if (xfhdr.filemode & FILE_READ)
	    continue;
	xfhdr.filemode |= FILE_READ;
	if (lseek(fd, i * (off_t)sizeof(fileheader_t), SEEK_SET) < 0)
	    break;
	write(fd, &xfhdr, sizeof(xfhdr));
    }

    close(fd);
    return DIRCHANGED;
}

static int
mail_unread(int ent, fileheader_t * fhdr, const char *direct)
{
    // this function may cause arguments, so please specify
    // if you want this to be enabled.
#ifdef USE_USER_MAIL_UNREAD
    if (fhdr && fhdr->filemode & FILE_READ)
    {
	fhdr->filemode &= ~FILE_READ;
	substitute_record(direct, fhdr, ent);
	return FULLUPDATE;
    }
#endif // USE_USER_MAIL_UNREAD
    return DONOTHING;
}

/* in boards/mail 回信給原作者，轉信站亦可 */
int
mail_reply(int ent, fileheader_t * fhdr, const char *direct)
{
    char            uid[STRLEN];
    FILE           *fp;
    char            genbuf[512];
    int		    oent = ent;

    stand_title("回  信");

    /* 判斷是 boards 或 mail */
    if (curredit & EDIT_MAIL)
	setuserfile(quote_file, fhdr->filename);
    else
	setbfile(quote_file, currboard, fhdr->filename);

    /* find the author */
    strlcpy(quote_user, fhdr->owner, sizeof(quote_user));
    if (strchr(quote_user, '.')) {
	char           *t;
	char *strtok_pos;
	genbuf[0] = '\0';
	if ((fp = fopen(quote_file, "r"))) {
	    fgets(genbuf, sizeof(genbuf), fp);
	    fclose(fp);
	}
	t = strtok_r(genbuf, str_space, &strtok_pos);
	if (t && (strcmp(t, str_author1)==0 || strcmp(t, str_author2)==0)
		&& (t=strtok_r(NULL, str_space, &strtok_pos)) != NULL)
	    strlcpy(uid, t, sizeof(uid));
	else {
	    vmsg("錯誤: 找不到作者。");
	    quote_user[0]='\0';
	    quote_file[0]='\0';
	    return FULLUPDATE;
	}
    } else
	strlcpy(uid, quote_user, sizeof(uid));

    /* make the title */
    do_reply_title(3, fhdr->title);
    prints("\n收信人: %s\n標  題: %s\n", uid, save_title);

    /* edit, then send the mail */
    ent = curredit;
    switch (do_send(uid, save_title)) {
    case -1:
	outs(err_uid);
	break;
    case -2:
	outs(msg_cancel);
	break;
    case -3:
	prints("使用者 [%s] 無法收信", uid);
	break;

    case 0:
	/* success */
	if (	direct &&	/* for board, no direct */
		(curredit & EDIT_MAIL) &&
		!(fhdr->filemode & FILE_REPLIED))
	{
	    fhdr->filemode |= FILE_REPLIED;
	    substitute_ref_record(direct, fhdr, oent);
	}
	break;
    }
    curredit = ent;
    pressanykey();
    quote_user[0]='\0';
    quote_file[0]='\0';
    if (strcasecmp(uid, cuser.userid) == 0)
	return DIRCHANGED;
    return FULLUPDATE;
}

static int
mail_edit(int ent, fileheader_t * fhdr, const char *direct)
{
    char            genbuf[200];

    if (!HasUserPerm(PERM_SYSOP))
	return DONOTHING;

    setdirpath(genbuf, direct, fhdr->filename);
    vedit(genbuf, NA, NULL);
    return FULLUPDATE;
}

static int
mail_nooutmail(int ent, fileheader_t * fhdr, const char *direct)
{
    cuser.uflag2 ^= REJ_OUTTAMAIL;
    passwd_update(usernum, &cuser);
    return FULLUPDATE;

}

static int
mail_mark(int ent, fileheader_t * fhdr, const char *direct)
{
    fhdr->filemode ^= FILE_MARKED;

    substitute_ref_record(direct, fhdr, ent);
    return PART_REDRAW;
}

/* help for mail reading */
static const char    * const mail_help[] = {
    "\0電子信箱操作說明",
    "\01基本命令",
    "(p/↑)(n/↓) 前一篇/下一篇文章",
    "(P)(PgUp)    前一頁",
    "(N)(PgDn)    下一頁",
    "(數字鍵)     跳到第 ## 筆",
    "($)          跳到最後一筆",
    "(r)(→)      讀信",
    "(R)/(y)      回信 / 群組回信",
    "\01進階命令",
    "(TAB)        切換顯示模式(目前有一般及顯示大小)",
    "(O)          關閉/開啟 站外信件轉入",
    "(c)/(z)      此信件收入私人信件夾/進入私人信件夾",
    "(x)/(X)      轉信給其它使用者/轉錄文章到其他看板",
    "(F)/(u)      將信傳送回您的電子信箱/水球整理寄回信箱",
    "(d)          殺掉此信",
    "(D)          殺掉指定範圍的信",
    "(m)          將信標記，以防被清除",
    "(^G)         立即重建信箱 (信箱毀損時用)",
    "(t)          標記欲刪除信件",
    "(^D)         刪除已標記信件",
    NULL
};

static int
m_help(void)
{
    show_help(mail_help);
    return FULLUPDATE;
}

static int
mail_cross_post(int ent, fileheader_t * fhdr, const char *direct)
{
    char            xboard[20], fname[80], xfpath[80], xtitle[80], inputbuf[10];
    fileheader_t    xfile;
    FILE           *xptr;
    int             author = 0;
    char            genbuf[200];
    char            genbuf2[4];

#if 0
    // 除非有人明白為何要先 ChekPostPerm 並修復，
    // 否則先 disable 這段 code - 目前常造成 crash。
    //
    // XXX (will crash sometimes because currborad is not defined yet)
    // 麻煩 in2 來修復這裡: 確認轉錄為何要先 CheckPostPerm
    if (!currboard || currboard[0] == 0)
    {
	enter_board(DEFAULT_BOARD);
    }
    assert(0<=ent-1 && ent-1<MAX_BOARD);
    
    if (!CheckPostPerm()) {
	vmsg("對不起，您目前無法轉錄文章！");
	return FULLUPDATE;
    }
#endif

    move(2, 0);
    clrtoeol();
    if (postrecord.times > 1)
    {
	outs(ANSI_COLOR(1;31) 
	"請注意: 若過量重複轉錄將視為洗板，導致被開罰單停權。\n" ANSI_RESET
	"若有特別需求請洽各板主，請他們幫你轉文。\n\n");
    }
    move(1, 0);
    CompleteBoard("轉錄本文章於看板：", xboard);

    if (*xboard == '\0' || !haspostperm(xboard))
    {
	vmsg("無法轉錄");
	return FULLUPDATE;
    }

    /* 借用變數 */
    ent = StringHash(fhdr->title);
    /* 同樣 title 不管對哪個板都算 cross post , 所以不用檢查 author */

    if ((ent != 0 && ent == postrecord.checksum[0])) {
	/* 檢查 cross post 次數 */
	if (postrecord.times++ > MAX_CROSSNUM)
	    anticrosspost();
    } else {
	postrecord.times = 0;
	postrecord.last_bid = 0;
	postrecord.checksum[0] = ent;
    }

    ent = getbnum(xboard);
    assert(0<=ent-1 && ent-1<MAX_BOARD);
    if (!CheckPostRestriction(ent))
    {
	vmsg("你不夠資深喔！ (可在看板內按大寫 I 查看限制)");
	return FULLUPDATE;
    }

#ifdef USE_COOLDOWN
   if(check_cooldown(&bcache[ent - 1]))
       return READ_REDRAW;
#endif

    ent = 1;
    if (HasUserPerm(PERM_SYSOP) || !strcmp(fhdr->owner, cuser.userid)) {
	getdata(2, 0, "(1)原文轉載 (2)舊轉錄格式？[1] ",
		genbuf, 3, DOECHO);
	if (genbuf[0] != '2') {
	    ent = 0;
	    getdata(2, 0, "保留原作者名稱嗎?[Y] ", inputbuf, 3, DOECHO);
	    if (inputbuf[0] != 'n' && inputbuf[0] != 'N')
		author = 1;
	}
    }
    if (ent)
	snprintf(xtitle, sizeof(xtitle), "[轉錄]%.66s", fhdr->title);
    else
	strlcpy(xtitle, fhdr->title, sizeof(xtitle));

    snprintf(genbuf, sizeof(genbuf), "採用原標題《%.60s》嗎?[Y] ", xtitle);
    getdata(2, 0, genbuf, genbuf2, sizeof(genbuf2), LCECHO);
    if (*genbuf2 == 'n')
	if (getdata(2, 0, "標題：", genbuf, TTLEN, DOECHO))
	    strlcpy(xtitle, genbuf, sizeof(xtitle));

    getdata(2, 0, "(S)存檔 (L)站內 (Q)取消？[Q] ", genbuf, 3, LCECHO);
    if (genbuf[0] == 'l' || genbuf[0] == 's') {
	int             currmode0 = currmode;

	currmode = 0;
	setbpath(xfpath, xboard);
	stampfile(xfpath, &xfile);
	if (author)
	    strlcpy(xfile.owner, fhdr->owner, sizeof(xfile.owner));
	else
	    strlcpy(xfile.owner, cuser.userid, sizeof(xfile.owner));
	strlcpy(xfile.title, xtitle, sizeof(xfile.title));
	if (genbuf[0] == 'l') {
	    xfile.filemode = FILE_LOCAL;
	}
	setuserfile(fname, fhdr->filename);
	{
	    const char *save_currboard;
	    xptr = fopen(xfpath, "w");
	    assert(xptr);

	    strlcpy(save_title, xfile.title, sizeof(save_title));
	    save_currboard = currboard;
	    currboard = xboard;
	    write_header(xptr, save_title);
	    currboard = save_currboard;

	    fprintf(xptr, "※ [本文轉錄自 %s 信箱]\n\n", cuser.userid);

	    b_suckinfile(xptr, fname);
	    addsignature(xptr, 0);
	    fclose(xptr);
	}

	setbdir(fname, xboard);
	append_record(fname, &xfile, sizeof(xfile));
	setbtotal(getbnum(xboard));

	if (!xfile.filemode)
	    outgo_post(&xfile, xboard, cuser.userid, cuser.nickname);
#ifdef USE_COOLDOWN
	if (bcache[getbnum(xboard) - 1].brdattr & BRD_COOLDOWN)
	    add_cooldowntime(usernum, 5);
	add_posttimes(usernum, 1);
#endif

	// cross-post does not add numpost.
	outs("轉錄信件不增加文章數，敬請包涵。");

	vmsg("文章轉錄完成");
	currmode = currmode0;
    }
    return FULLUPDATE;
}

int
mail_man(void)
{
    char            buf[PATHLEN], buf1[64];
    int             mode0 = currutmp->mode;
    int             stat0 = currstat;

    sethomeman(buf, cuser.userid);
    snprintf(buf1, sizeof(buf1), "%s 的信件夾", cuser.userid);
    a_menu(buf1, buf, HasUserPerm(PERM_MAILLIMIT) ? 1 : 0, 0, NULL);
    currutmp->mode = mode0;
    currstat = stat0;
    return FULLUPDATE;
}

// XXX BUG mail_cite 有可能會跳進 a_menu, 而 a_menu 會 check
// currbid。 一整個糟糕的邏輯錯誤...
static int
mail_cite(int ent, fileheader_t * fhdr, const char *direct)
{
    char            fpath[PATHLEN];
    char            title[TTLEN + 1];
    static char     xboard[20];
    char            buf[20];
    int             bid;

    setuserfile(fpath, fhdr->filename);
    strlcpy(title, "◇ ", sizeof(title));
    strlcpy(title + 3, fhdr->title, sizeof(title) - 3);
    a_copyitem(fpath, title, 0, 1);

    if (cuser.userlevel >= PERM_BM) {
	move(2, 0);
	clrtoeol();
	move(3, 0);
	clrtoeol();
	move(1, 0);

	CompleteBoard("輸入看板名稱 (直接Enter進入私人信件夾)：", buf);
	if (*buf)
	    strlcpy(xboard, buf, sizeof(xboard));
	if (*xboard && ((bid = getbnum(xboard)) > 0)){ /* XXXbid */
	    setapath(fpath, xboard);
	    setutmpmode(ANNOUNCE);
	    a_menu(xboard, fpath, 
		    HasUserPerm(PERM_ALLBOARD) ? 2 : is_BM_cache(bid) ? 1 : 0,
		    bid,
		   NULL);
	} else {
	    mail_man();
	}
	return FULLUPDATE;
    } else {
	mail_man();
	return FULLUPDATE;
    }
}

static int
mail_save(int ent, fileheader_t * fhdr, const char *direct)
{
    char            fpath[PATHLEN];
    char            title[TTLEN + 1];

    if (HasUserPerm(PERM_MAILLIMIT)) {
	setuserfile(fpath, fhdr->filename);
	strlcpy(title, "◇ ", sizeof(title));
	strlcpy(title + 3, fhdr->title, sizeof(title) - 3);
	a_copyitem(fpath, title, fhdr->owner, 1);
	sethomeman(fpath, cuser.userid);
	a_menu(cuser.userid, fpath, 1, 0, NULL);
	return FULLUPDATE;
    }
    return DONOTHING;
}

#ifdef OUTJOBSPOOL
static int
mail_waterball(int ent, fileheader_t * fhdr, const char *direct)
{
    static char     address[60] = "", cmode = 1;
    char            fname[500], genbuf[200];
    FILE           *fp;

    if (!(strstr(fhdr->title, "熱線") && strstr(fhdr->title, "記錄"))) {
	vmsg("必須是 熱線記錄 才能使用水球整理的唷!");
	return 1;
    }

    if (!address[0])
	strlcpy(address, cuser.email, sizeof(address));

    move(b_lines - 8, 0); clrtobot();
    outs(ANSI_COLOR(1;33;45) "★水球整理程式 " ANSI_RESET "\n"
	 "系統將會按照和不同人丟的水球各自獨立\n"
	 "於整點的時候 (尖峰時段除外) 將資料整理好寄送給您\n\n\n");

    if (address[0]) {
	snprintf(genbuf, sizeof(genbuf), "寄往 [%s] 嗎[Y/n/q]？ ", address);
	getdata(b_lines - 5, 0, genbuf, fname, 3, LCECHO);
	if (fname[0] == 'q') {
	    outmsg("取消處理");
	    return 1;
	}
	if (fname[0] == 'n')
	    address[0] = '\0';
    }
    if (!address[0]) {
	move(b_lines-4, 0);
	prints(   "請注意目前只支援寄往標準 e-mail 地址。\n"
		"若想寄回此信箱請用輸入 %s.bbs@" MYHOSTNAME "\n", cuser.userid);

	getdata(b_lines - 5, 0, "請輸入郵件地址：", fname, 60, DOECHO);
	if (fname[0] && strchr(fname, '.')) {
	    strlcpy(address, fname, sizeof(address));
	} else {
	    vmsg("地址格式不正確，取消處理");
	    return 1;
	}
    }
    trim(address);
    if (invalidaddr(address))
	return -2;
    move(b_lines-4, 0); clrtobot();

    if( strstr(address, ".bbs") && REJECT_OUTTAMAIL ){
	outs("\n您必須要打開接受站外信, 水球整理系統才能寄入結果\n"
	     "請麻煩到【郵件選單】按大寫 O改成接受站外信 (在右上角)\n"
	     "再重新執行本功\能 :)\n");
	vmsg("請打開站外信, 再重新執行本功\能");
	return FULLUPDATE;
    }

    //snprintf(fname, sizeof(fname), "%d\n", cmode);
    outs("系統提供兩種模式: \n"
	 "模式 0: 精簡模式, 將不含顏色控制碼, 方便以純文字編輯器整理收藏\n"
	 "模式 1: 華麗模式, 包含顏色控制碼等, 方便在 bbs上直接編輯收藏\n");
    getdata(b_lines - 1, 0, "使用模式(0/1/Q)? [1]", fname, 3, LCECHO);
    if (fname[0] == 'Q' || fname[0] == 'q') {
	outmsg("取消處理");
	return FULLUPDATE;
    }
    cmode = (fname[0] != '0' && fname[0] != '1') ? 1 : fname[0] - '0';

    snprintf(fname, sizeof(fname), BBSHOME "/jobspool/water.src.%s-%d",
	     cuser.userid, (int)now);
    snprintf(genbuf, sizeof(genbuf), "cp " BBSHOME "/home/%c/%s/%s %s",
	     cuser.userid[0], cuser.userid, fhdr->filename, fname);
    system(genbuf);
    /* dirty code ;x */
    snprintf(fname, sizeof(fname), BBSHOME "/jobspool/water.des.%s-%d",
	     cuser.userid, (int)now);
    fp = fopen(fname, "wt");
    assert(fp);
    fprintf(fp, "%s\n%s\n%d\n", cuser.userid, address, cmode);
    fclose(fp);
    vmsg("設定完成, 系統將在下一個整點(尖峰時段除外)將資料寄給您");
    return FULLUPDATE;
}
#endif
static const onekey_t mail_comms[] = {
    { 0, NULL }, // Ctrl('A')
    { 0, NULL }, // Ctrl('B')
    { 0, NULL }, // Ctrl('C')
    { 0, NULL }, // Ctrl('D')
    { 0, NULL }, // Ctrl('E')
    { 0, NULL }, // Ctrl('F')
    { 0, built_mail_index }, // Ctrl('G')
    { 0, NULL }, // Ctrl('H')
    { 0, toggle_showmail_mode }, // Ctrl('I') 
    { 0, NULL }, // Ctrl('J')
    { 0, NULL }, // Ctrl('K')
    { 0, NULL }, // Ctrl('L')
    { 0, NULL }, // Ctrl('M')
    { 0, NULL }, // Ctrl('N')
    { 0, NULL }, // Ctrl('O')	// DO NOT USE THIS KEY - UNIX not sending
    { 0, NULL }, // Ctrl('P')
    { 0, NULL }, // Ctrl('Q')
    { 0, NULL }, // Ctrl('R')
    { 0, NULL }, // Ctrl('S')
    { 0, NULL }, // Ctrl('T')
    { 0, NULL }, // Ctrl('U')
    { 0, NULL }, // Ctrl('V')
    { 0, NULL }, // Ctrl('W')
    { 0, NULL }, // Ctrl('X')
    { 0, NULL }, // Ctrl('Y')
    { 0, NULL }, // Ctrl('Z') 26
    { 0, NULL }, { 0, NULL }, { 0, NULL }, { 0, NULL }, { 0, NULL },
    { 0, NULL }, { 0, NULL }, { 0, NULL }, { 0, NULL }, { 0, NULL },
    { 0, NULL }, { 0, NULL }, { 0, NULL }, { 0, NULL }, { 0, NULL },
    { 0, NULL }, { 0, NULL }, { 0, NULL }, { 0, NULL }, { 0, NULL },
    { 0, NULL }, { 0, NULL }, { 0, NULL }, { 0, NULL }, { 0, NULL },
    { 0, NULL }, { 0, NULL }, { 0, NULL }, { 0, NULL }, { 0, NULL },
    { 0, NULL }, { 0, NULL }, { 0, NULL }, { 0, NULL }, { 0, NULL },
    { 0, NULL }, { 0, NULL }, { 0, NULL },
    { 0, NULL }, // 'A' 65
    { 0, NULL }, // 'B'
    { 0, NULL }, // 'C'
    { 1, del_range }, // 'D'
    { 1, mail_edit }, // 'E'
    { 0, NULL }, // 'F'
    { 0, NULL }, // 'G'
    { 0, NULL }, // 'H'
    { 0, NULL }, // 'I'
    { 0, NULL }, // 'J'
    { 0, NULL }, // 'K'
    { 0, NULL }, // 'L'
    { 0, NULL }, // 'M'
    { 0, NULL }, // 'N'
    { 1, mail_nooutmail }, // 'O'
    { 0, NULL }, // 'P'
    { 0, NULL }, // 'Q'
    { 1, mail_reply }, // 'R'
    { 0, NULL }, // 'S'
    { 1, edit_title }, // 'T'
    { 0, NULL }, // 'U'
    { 1, mail_unread }, // 'V'
    { 0, NULL }, // 'W'
    { 1, mail_cross_post }, // 'X'
    { 0, NULL }, // 'Y'
    { 0, NULL }, // 'Z' 90
    { 0, NULL }, { 0, NULL }, { 0, NULL }, { 0, NULL }, { 0, NULL }, { 0, NULL },
    { 0, NULL }, // 'a' 97
#ifdef OLD_GEM_SUPPORT
    { 0, mail_gem }, // 'b'
#else
    { 0, NULL }, // 'b'
#endif
    { 1, mail_cite }, // 'c'
    { 1, mail_del }, // 'd'
    { 0, NULL }, // 'e'
    { 0, NULL }, // 'f'
    { 0, NULL }, // 'g'
    { 0, m_help }, // 'h'
    { 0, NULL }, // 'i'
    { 0, NULL }, // 'j'
    { 0, NULL }, // 'k'
    { 0, NULL }, // 'l'
    { 1, mail_mark }, // 'm'
    { 0, NULL }, // 'n'
    { 0, NULL }, // 'o'
    { 0, NULL }, // 'p'
    { 0, NULL }, // 'q'
    { 1, mail_read }, // 'r'
    { 1, mail_save }, // 's'
    { 0, NULL }, // 't'
#ifdef OUTJOBSPOOL
    { 1, mail_waterball }, // 'u'
#else
    { 0, NULL }, // 'u'
#endif
    { 0, mail_read_all }, // 'v'
    { 1, b_call_in }, // 'w'
    { 1, m_forward }, // 'x'
    { 1, multi_reply }, // 'y'
    { 0, mail_man }, // 'z' 122
};

int
m_read(void)
{
    int back_bid;
    if (get_num_records(currmaildir, sizeof(fileheader_t))) {
	curredit = EDIT_MAIL;
	curredit &= ~EDIT_ITEM;
	back_bid = currbid;
	currbid = 0;
	i_read(RMAIL, currmaildir, mailtitle, maildoent, mail_comms, -1);
	currbid = back_bid;
	curredit = 0;
	setmailalert();
	return 0;
    } else {
	outs("您沒有來信");
	return XEASY;
    }
}

/* 寄站內信 */
static int
send_inner_mail(const char *fpath, const char *title, const char *receiver)
{
    char            fname[PATHLEN];
    fileheader_t    mymail;
    char            rightid[IDLEN+1];

    if (!searchuser(receiver, rightid))
	return -2;

    /* to avoid DDOS of disk */
    sethomedir(fname, rightid);
    if (strcmp(rightid, cuser.userid) == 0) {
	if (chk_mailbox_limit())
	    return -4;
    }

    sethomepath(fname, rightid);
    stampfile(fname, &mymail);
    if (!strcmp(rightid, cuser.userid)) {
	/* Using BBSNAME may be too loooooong. */
	strlcpy(mymail.owner, "[站內]", sizeof(mymail.owner));
	mymail.filemode = FILE_READ;
    } else
	strlcpy(mymail.owner, cuser.userid, sizeof(mymail.owner));
    strlcpy(mymail.title, title, sizeof(mymail.title));
    unlink(fname);
    Copy(fpath, fname);
    sethomedir(fname, rightid);
    append_record_forward(fname, &mymail, sizeof(mymail), rightid);
    sendalert(receiver, ALERT_NEW_MAIL);
    return 0;
}

#include <netdb.h>
#include <pwd.h>
#include <time.h>

#ifndef USE_BSMTP
static int
bbs_sendmail(const char *fpath, const char *title, char *receiver)
{
    char           *ptr;
    char            genbuf[256];
    FILE           *fin, *fout;

    /* 中途攔截 */
    if ((ptr = strchr(receiver, ';'))) {
	*ptr = '\0';
    }
    if ((ptr = strstr(receiver, str_mail_address)) || !strchr(receiver, '@')) {
	char            hacker[20];
	int             len;

	if (strchr(receiver, '@')) {
	    len = ptr - receiver;
	    memcpy(hacker, receiver, len);
	    hacker[len] = '\0';
	} else
	    strlcpy(hacker, receiver, sizeof(hacker));
	return send_inner_mail(fpath, title, hacker);
    }
    /* Running the sendmail */
    if (fpath == NULL) {
	snprintf(genbuf, sizeof(genbuf),
		 "/usr/sbin/sendmail %s > /dev/null", receiver);
	fin = fopen("etc/confirm", "r");
    } else {
	snprintf(genbuf, sizeof(genbuf),
		 "/usr/sbin/sendmail -f %s%s %s > /dev/null",
		 cuser.userid, str_mail_address, receiver);
	fin = fopen(fpath, "r");
    }
    if (fin == NULL)
	return -1;
    fout = popen(genbuf, "w");
    if (fout == NULL) {
	fclose(fin);
	return -1;
    }

    if (fpath)
	fprintf(fout, "Reply-To: %s%s\nFrom: %s <%s%s>\n",
		cuser.userid, str_mail_address,
		cuser.nickname,
		cuser.userid, str_mail_address);
    fprintf(fout,"To: %s\nSubject: %s\n"
		 "Mime-Version: 1.0\r\n"
		 "Content-Type: text/plain; charset=\"big5\"\r\n"
		 "Content-Transfer-Encoding: 8bit\r\n"
		 "X-Disclaimer: " BBSNAME "對本信內容恕不負責。\n\n",
		receiver, title);

    while (fgets(genbuf, sizeof(genbuf), fin)) {
	if (genbuf[0] == '.' && genbuf[1] == '\n')
	    fputs(". \n", fout);
	else
	    fputs(genbuf, fout);
    }
    fclose(fin);
    fprintf(fout, ".\n");
    pclose(fout);
    return 0;
}
#else				/* USE_BSMTP */

int
bsmtp(const char *fpath, const char *title, const char *rcpt)
{
    char            buf[80], *ptr;
    time4_t         chrono;
    MailQueue       mqueue;

    /* check if the mail is a inner mail */
    if ((ptr = strstr(rcpt, str_mail_address)) || !strchr(rcpt, '@')) {
	char            hacker[20];
	int             len;

	if (strchr(rcpt, '@')) {
	    len = ptr - rcpt;
	    memcpy(hacker, rcpt, len);
	    hacker[len] = '\0';
	} else
	    strlcpy(hacker, rcpt, sizeof(hacker));
	return send_inner_mail(fpath, title, hacker);
    }
    chrono = now;

    /* stamp the queue file */
    strlcpy(buf, "out/", sizeof(buf));
    for (;;) {
	snprintf(buf + 4, sizeof(buf) - 4, "M.%d.%d.A", (int)++chrono, getpid());
	if (!dashf(buf)) {
	    Copy(fpath, buf);
	    break;
	}
    }

    fpath = buf;

    /* setup mail queue */
    mqueue.mailtime = chrono;
    // XXX (unused) mqueue.method = method;
    strlcpy(mqueue.filepath, fpath, sizeof(mqueue.filepath));
    strlcpy(mqueue.subject, title, sizeof(mqueue.subject));
    strlcpy(mqueue.sender, cuser.userid, sizeof(mqueue.sender));
    strlcpy(mqueue.username, cuser.nickname, sizeof(mqueue.username));
    strlcpy(mqueue.rcpt, rcpt, sizeof(mqueue.rcpt));

    if (append_record("out/" FN_DIR, (fileheader_t *) & mqueue, sizeof(mqueue)) < 0)
	return 0;
    return chrono;
}
#endif				/* USE_BSMTP */

int
doforward(const char *direct, const fileheader_t * fh, int mode)
{
    static char     address[STRLEN] = "";
    char            fname[PATHLEN];
    char            genbuf[PATHLEN];
    int             return_no;

    if (!address[0])
     	strlcpy(address, cuser.email, sizeof(address));

    if( mode == 'U' ){
	vmsg("將進行 uuencode 。若您不清楚什麼是 uuencode 請改用 F轉寄。");
    }
    trim(address);

    if (address[0]) {
	snprintf(genbuf, sizeof(genbuf),
		 "確定轉寄給 [%s] 嗎(Y/N/Q)？[Y] ", address);
	getdata(b_lines, 0, genbuf, fname, 3, LCECHO);

	if (fname[0] == 'q') {
	    outmsg("取消轉寄");
	    return 1;
	}
	if (fname[0] == 'n')
	    address[0] = '\0';
    }
    if (!address[0]) {
	do {
	    getdata(b_lines - 1, 0, "請輸入轉寄地址：", fname, 60, DOECHO);
	    if (fname[0]) {
		if (strchr(fname, '.'))
		    strlcpy(address, fname, sizeof(address));
		else
		    snprintf(address, sizeof(address),
                    	"%s.bbs@%s", fname, MYHOSTNAME);
	    } else {
		vmsg("取消轉寄");
		return 1;
	    }
	} while (mode == 'Z' && strstr(address, MYHOSTNAME));
    }
    /* according to our experiment, many users leave blanks */
    trim(address);
    if (invalidaddr(address))
	return -2;

    outmsg("正轉寄請稍候...");
    refresh();

    /* 追蹤使用者 */
    if (HasUserPerm(PERM_LOGUSER)) 
	log_user("mailforward to %s ",address);
    if (mode == 'Z') {
	snprintf(fname, sizeof(fname),
		 TAR_PATH " cfz /tmp/home.%s.tgz home/%c/%s; "
		 MUTT_PATH " -a /tmp/home.%s.tgz -s 'home.%s.tgz' -- '%s' </dev/null;"
		 "rm /tmp/home.%s.tgz",
		 cuser.userid, cuser.userid[0], cuser.userid,
		 cuser.userid, cuser.userid, address, cuser.userid);
	system(fname);
	return 0;
	snprintf(fname, sizeof(fname), TAR_PATH " cfz - home/%c/%s | "
		"/usr/bin/uuencode %s.tgz > %s",
		cuser.userid[0], cuser.userid, cuser.userid, direct);
	system(fname);
	strlcpy(fname, direct, sizeof(fname));
    } else if (mode == 'U') {
	char            tmp_buf[128];

	snprintf(fname, sizeof(fname), "/tmp/bbs.uu%05d", (int)currpid);
	snprintf(tmp_buf, sizeof(tmp_buf),
		 "/usr/bin/uuencode %s/%s uu.%05d > %s",
		 direct, fh->filename, (int)currpid, fname);
	system(tmp_buf);
    } else if (mode == 'F') {
	char            tmp_buf[128];

	snprintf(fname, sizeof(fname), "/tmp/bbs.f%05d", (int)currpid);
	snprintf(tmp_buf, sizeof(tmp_buf), "%s/%s", direct, fh->filename);
	Copy(tmp_buf, fname);
    } else
	return -1;

    return_no =
#ifndef USE_BSMTP
	bbs_sendmail(fname, fh->title, address);
#else
	bsmtp(fname, fh->title, address);
#endif
    unlink(fname);
    return (return_no);
}

int
load_mailalert(const char *userid)
{
    struct stat     st;
    char            maildir[MAXPATHLEN];
    int             fd;
    register int    num;
    fileheader_t    my_mail;

    sethomedir(maildir, userid);
    if (!HasUserPerm(PERM_BASIC))
	return 0;
    if (stat(maildir, &st) < 0)
	return 0;
    num = st.st_size / sizeof(fileheader_t);
    if (num <= 0)
	return 0;
    if (num > NEWMAIL_CHECK_RANGE) 
	num = NEWMAIL_CHECK_RANGE;

    /* 看看有沒有信件還沒讀過？從檔尾回頭檢查，效率較高 */
    if ((fd = open(maildir, O_RDONLY)) > 0) {
	lseek(fd, st.st_size - sizeof(fileheader_t), SEEK_SET);
	while (num--) {
	    read(fd, &my_mail, sizeof(fileheader_t));
	    if (!(my_mail.filemode & FILE_READ)) {
		close(fd);
		return ALERT_NEW_MAIL;
	    }
	    lseek(fd, -(off_t) 2 * sizeof(fileheader_t), SEEK_CUR);
	}
	close(fd);
    }
    return 0;
}
