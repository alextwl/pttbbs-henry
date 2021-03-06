/* $Id: openvice.c 2725 2005-05-16 18:36:27Z kcwu $ */
/* 發票開獎小程式 */

#include "bbs.h"

#define VICE_SHOW  BBSHOME "/etc/vice.show1"
#define VICE_BINGO BBSHOME "/etc/vice.bingo"
#define VICE_NEW   "vice.new"
#define VICE_DATA  "vice.data"
#define MAX_BINGO  99999999

int main(int argc, char **argv)
{
    char *TABLE[5] = {"一", "二", "三", "四", "五"};
    int i = 0, bingo, base = 0;


    FILE *fp = fopen(VICE_SHOW, "w"), *fb = fopen(VICE_BINGO, "w");

    // XXX: resolve_utmp();
    attach_SHM();

    srandom(SHM->number);
    /* FIXME 小站的 SHM->number 變化不大, 可能導致開獎號碼固定 */

    if (!fp || !fb )
	perror("error open file");


    bingo = random() % MAX_BINGO;
    fprintf(fp, "%1c[1;33m統一發票中獎號碼[m\n", ' ');
    fprintf(fp, "%1c[1;37m================[m\n", ' ');
    fprintf(fp, "%1c[1;31m特別獎[m: [1;31m%08d[m\n\n", ' ', bingo);
    fprintf(fb, "%d\n", bingo);

    while (i < 5)
    {
	bingo = (base + random()) % MAX_BINGO;
	fprintf(fp, "%1c[1;36m第%s獎[m: [1;37m%08d[m\n", ' ', TABLE[i], bingo);
	fprintf(fb, "%08d\n", bingo);
	i++;
    }
    fclose(fp);
    fclose(fb);
    return 0;
}
