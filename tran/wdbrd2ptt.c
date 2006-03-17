/*
 * 2006 Kanru Chen, Henry BBS @ NCNU CSIE
 * WD .BOARDS 到 PTT .BRD 轉換程式
 */
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "wdbrd2ptt.h"

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
	wdboardheader_t wd;
	pttboardheader_t ptt;

	while (read (wdfd, &wd, sizeof (wdboardheader_t)) == sizeof (wdboardheader_t)) {
		memset (&ptt, 0, sizeof (pttboardheader_t));

		memcpy (ptt.brdname, wd.brdname, sizeof (ptt.brdname));
		memcpy (ptt.title, wd.title, sizeof (ptt.title));
		memcpy (ptt.BM, wd.BM, sizeof (ptt.BM));
		ptt.brdattr = wd.brdattr & (BRD_NOCOUNT|BRD_NOTRAN|BRD_GROUPBOARD|BRD_HIDE
						|BRD_POSTMASK|BRD_ANONYMOUS);
		ptt.bupdate = wd.bupdate;
		ptt.bvote = wd.bvote;
		ptt.vtime = wd.vtime;
		ptt.nuser = wd.totalvisit;
		
		write (pttfd, &ptt, sizeof (pttboardheader_t));
		count++;
	}

	printf ("Translate %d records\n", count);
	close (pttfd);
	close (wdfd);
	
	return 0;
}
