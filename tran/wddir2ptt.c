/*
 * 2006 Kanru Chen, Henry BBS @ NCNU CSIE
 * WD 到 PTT .DIR 轉換程式
 */
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "wddir2ptt.h"

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
	wdfileheader_t wd;
	pttfileheader_t ptt;

	while (read (wdfd, &wd, sizeof (wdfileheader_t)) == sizeof (wdfileheader_t)) {
		memset (&ptt, 0, sizeof (pttfileheader_t));

		memcpy (ptt.filename, wd.filename, sizeof (ptt.filename));
		ptt.filename[FNLEN-1]='\0';
		/* duno textlen */
		memcpy (ptt.owner, wd.owner, sizeof (ptt.owner));
		memcpy (ptt.date, wd.date, sizeof (ptt.date));
		memcpy (ptt.title, wd.title, sizeof (ptt.title));
		ptt.title[TTLEN]='\0';
		ptt.recommend = wd.score;
		ptt.filemode = wd.filemode & (FILE_LOCAL|FILE_READ|FILE_MARKED
						|FILE_DIGEST);
#if 0
		if (wd.filemode & 0x90) /* bottom */
			ptt.filemode |= FILE_BOTTOM;
#endif
		if (wd.filemode & 0x10) /* reply */
			ptt.filemode |= FILE_REPLIED;

		write (pttfd, &ptt, sizeof (pttfileheader_t));
		count++;
	}

	printf ("Translate %d records\n", count);
	close (pttfd);
	close (wdfd);
	
	return 0;
}
