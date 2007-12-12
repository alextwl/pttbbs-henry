/* $Id: more.c 3669 2007-12-12 01:37:19Z kcwu $ */
#include "bbs.h"

/* use new pager: piaip's more. */
int more(char *fpath, int promptend)
{
    return pmore(fpath, promptend);
}

