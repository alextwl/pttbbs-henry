/*
 * 2006 Kanru Chen, Henry BBS @ NCNU CSIE
 * WD 到 PTT .PASSWDS 轉換程式
 * 用法：
 * bin/wdusr2ptt OLDPASSWDS NEWPASSWDS
 * bin/merge_passwd .PASSWDS NEWPASSWDS .PASSWDS
 * mv NEWPASSWDS .PASSWDS
 * bin/tunepasswd
 */
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "wdusr2ptt.h"

int main (int argc, char *argv[])
{
	if (argc < 3) {
		fprintf (stderr, "Usage: %s [input] [output]\n", argv[0]);
		return 1;
	}

	mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP;
	int wdfd = open (argv[1], O_RDONLY);
	int pttfd = open (argv[2], O_WRONLY|O_CREAT, mode);
	int count = 0;
	wduserec_t wd;
	pttuserec_t ptt;

#define PASSWD_VERSION	2275

	while (read (wdfd, &wd, sizeof (wduserec_t)) == sizeof (wduserec_t)) {
		memset (&ptt, 0, sizeof (pttuserec_t));

		/* begin to copy data */
		ptt.version = PASSWD_VERSION;
		memcpy (ptt.userid, wd.userid, sizeof (ptt.userid));
		memcpy (ptt.realname, wd.realname, sizeof (ptt.realname));
		memcpy (ptt.nickname, wd.username, sizeof (ptt.nickname));
		memcpy (ptt.passwd, wd.passwd, sizeof (ptt.passwd));
		ptt.uflag = wd.uflag;
		ptt.userlevel = wd.userlevel;
		ptt.numlogins = wd.numlogins;
		ptt.numposts = wd.numposts;
		ptt.firstlogin = wd.firstlogin;
		ptt.lastlogin = wd.lastlogin;
		memcpy (ptt.lasthost, wd.lasthost, sizeof(ptt.lasthost));
		ptt.money = wd.goldmoney / 100;
		memcpy (ptt.email, wd.email, sizeof(wd.email));
		memcpy (ptt.address, wd.address, sizeof(ptt.address));
		memcpy (ptt.justify, wd.justify, sizeof(ptt.justify));
		ptt.month = wd.month;
		ptt.day = wd.day;
		ptt.year = wd.year;
		ptt.sex = wd.sex;
		ptt.state = wd.state;
		ptt.pager = wd.pager;
		ptt.invisible = wd.invisible;
		ptt.exmailbox = wd.exmailbox;
		memcpy (ptt.mind, wd.feeling, sizeof (ptt.mind));

		write (pttfd, &ptt, sizeof (pttuserec_t));
		count++;
	}

	printf ("Translate %d records\n", count);
	close (pttfd);
	close (wdfd);
	
	return 0;
}
