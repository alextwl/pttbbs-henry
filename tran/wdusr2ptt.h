#define IDLEN      12
#define PASSLEN    14             /* Length of encrypted passwd field */
#define REGLEN     38             /* Length of registration data */

#define PASSWD_VERSION	2275
#define time4_t time_t

typedef struct chicken_t {
    char    name[20];
    char    type;             /* ���� */
    unsigned char   tech[16]; /* �ޯ� */
    time4_t birthday;         /* �ͤ� */
    time4_t lastvisit;        /* �W�����U�ɶ� */
    int     oo;               /* �ɫ~ */
    int     food;             /* ���� */
    int     medicine;         /* �ī~ */
    int     weight;           /* �魫 */
    int     clean;            /* ���b */
    int     run;              /* �ӱ��� */
    int     attack;           /* �����O */
    int     book;             /* ���� */
    int     happy;            /* �ּ� */
    int     satis;            /* ���N�� */
    int     temperament;      /* ��� */
    int     tiredstrong;      /* �h�ҫ� */
    int     sick;             /* �f����� */
    int     hp;               /* ��q */
    int     hp_max;           /* ����q */
    int     mm;               /* �k�O */
    int     mm_max;           /* ���k�O */
    time4_t cbirth;           /* ��ڭp��Ϊ��ͤ� */
    int     pad[2];           /* �d�ۥH��� */
} chicken_t;

typedef struct pttuserec_t {
    unsigned int    version;	/* version number of this sturcture, we
    				 * use revision number of project to denote.*/

    char    userid[IDLEN + 1];	/* ID */
    char    realname[20];	/* �u��m�W */
    char    nickname[24];	/* �ʺ� */
    char    passwd[PASSLEN];	/* �K�X */
    unsigned int    uflag;	/* �ߺD1 */
    unsigned int    uflag2;	/* �ߺD2 */
    unsigned int    userlevel;	/* �v�� */
    unsigned int    numlogins;	/* �W������ */
    unsigned int    numposts;	/* �峹�g�� */
    time4_t firstlogin;		/* ���U�ɶ� */
    time4_t lastlogin;		/* �̪�W���ɶ� */
    char    lasthost[16];	/* �W���W���ӷ� */
    int     money;		/* Ptt�� */
    char    remoteuser[3];	/* �O�d �ثe�S�Ψ쪺 */
    char    proverb;		/* �y�k�� (unused) */
    char    email[50];		/* Email */
    char    address[50];	/* ��} */
    char    justify[REGLEN + 1];    /* �f�ָ�� */
    unsigned char   month;	/* �ͤ� �� */
    unsigned char   day;	/* �ͤ� �� */
    unsigned char   year;	/* �ͤ� �~ */
    unsigned char   sex;	/* �ʧO */
    unsigned char   state;	/* TODO unknown (unused ?) */
    unsigned char   pager;	/* �I�s�����A */
    unsigned char   invisible;	/* ���Ϊ��A */
    unsigned int    exmailbox;	/* �ʶR�H�c�� TODO short �N���F */
    chicken_t       mychicken;	/* �d�� */
    time4_t lastsong;		/* �W���I�q�ɶ� */
    unsigned int    loginview;	/* �i���e�� */
    unsigned char   channel;	/* TODO unused */
    unsigned short  vl_count;	/* �H�k�O�� ViolateLaw counter */
    unsigned short  five_win;	/* ���l�Ѿ��Z �� */
    unsigned short  five_lose;	/* ���l�Ѿ��Z �� */
    unsigned short  five_tie;	/* ���l�Ѿ��Z �M */
    unsigned short  chc_win;	/* �H�Ѿ��Z �� */
    unsigned short  chc_lose;	/* �H�Ѿ��Z �� */
    unsigned short  chc_tie;	/* �H�Ѿ��Z �M */
    int     mobile;		/* ������X */
    char    mind[4];		/* �߱� not a null-terminate string */
    char    pad0[11];		/* �q�e�� ident �����Ҧr���A�{�b�i�H���Ӱ��O���ƤF�A
				   ���L�̦n�O�o�n���M�� 0 */
    unsigned char   signature;	/* �D��ñ�W�� */

    unsigned char   goodpost;	/* �������n�峹�� */
    unsigned char   badpost;	/* �������a�峹�� */
    unsigned char   goodsale;	/* �v�� �n������  */
    unsigned char   badsale;	/* �v�� �a������  */
    char    myangel[IDLEN+1];	/* �ڪ��p�Ѩ� */
    unsigned short  chess_elo_rating;	/* �H�ѵ��Ť� */
    unsigned int    withme;	/* �ڷQ��H�U�ѡA���.... */
    time4_t timeremovebadpost;  /* �W���R���H��ɶ� */
    char    pad[30];
} pttuserec_t;
/* these are flags in userec_t.uflag */
#define PAGER_FLAG      0x4     /* true if pager was OFF last session */
#define CLOAK_FLAG      0x8     /* true if cloak was ON last session */
#define FRIEND_FLAG     0x10    /* true if show friends only */
#define BRDSORT_FLAG    0x20    /* true if the boards sorted alphabetical */
#define MOVIE_FLAG      0x40    /* true if show movie */

/* useless flag */
//#define COLOR_FLAG      0x80    /* true if the color mode open */
//#define MIND_FLAG       0x100   /* true if mind search mode open <-Heat*/

#define DBCSAWARE_FLAG	0x200	/* true if DBCS-aware enabled. */
/* please keep this even if you don't have DBCSAWARE features turned on */

/* these are flags in userec_t.uflag2 */
#define WATER_MASK      000003  /* water mask */
#define WATER_ORIG      0x0
#define WATER_NEW       0x1
#define WATER_OFO       0x2
#define WATERMODE(mode) ((cuser.uflag2 & WATER_MASK) == mode)
#define FAVNOHILIGHT    0x10   /* false if hilight favorite */
#define FAVNEW_FLAG     0x20   /* true if add new board into one's fav */
#define FOREIGN         0x100  /* true if a foreign */
#define LIVERIGHT       0x200  /* true if get "liveright" already */
#define REJ_OUTTAMAIL   0x400 /* true if don't accept outside mails */
#define REJECT_OUTTAMAIL (cuser.uflag2 & REJ_OUTTAMAIL)

/* flags in userec_t.withme */
#define WITHME_ALLFLAG	0x55555555
#define WITHME_TALK	0x00000001
#define WITHME_NOTALK	0x00000002
#define WITHME_FIVE	0x00000004
#define WITHME_NOFIVE	0x00000008
#define WITHME_PAT	0x00000010
#define WITHME_NOPAT	0x00000020
#define WITHME_CHESS	0x00000040
#define WITHME_NOCHESS	0x00000080
#define WITHME_DARK	0x00000100
#define WITHME_NODARK	0x00000200
#define WITHME_GO	0x00000400
#define WITHME_NOGO	0x00000800

/* ------ WD ------ */
struct wduserec_t
{
  char userid[IDLEN + 1];         /* �ϥΪ̦W��  13 bytes */
  char realname[20];              /* �u��m�W    20 bytes */
  char username[24];              /* �ʺ�        24 bytes */
  char passwd[PASSLEN];           /* �K�X        14 bytes */
  unsigned char uflag;                   /* �ϥΪ̿ﶵ   1 byte  */
  unsigned int userlevel;                /* �ϥΪ��v��   4 bytes */
  ushort numlogins;               /* �W������     2 bytes */
  ushort numposts;                /* POST����     2 bytes */
  time_t firstlogin;              /* ���U�ɶ�     4 bytes */
  time_t lastlogin;               /* �e���W��     4 bytes */
  char lasthost[24];              /* �W���a�I    24 bytes */
  char vhost[24];                 /* �������}    24 bytes */
  char email[50];                 /* E-MAIL      50 bytes */
  char address[50];               /* �a�}        50 bytes */
  char justify[REGLEN + 1];       /* ���U���    39 bytes */
  unsigned char month;                   /* �X�ͤ��     1 byte  */
  unsigned char day;                     /* �X�ͤ��     1 byte  */
  unsigned char year;                    /* �X�ͦ~��     1 byte  */
  unsigned char sex;                     /* �ʧO         1 byte  */
  unsigned char state;                   /* ���A??       1 byte  */
  unsigned int habit;                    /* �ߦn�]�w     4 bytes */
  unsigned char pager;                   /* �߱��C��     1 bytes */
  unsigned char invisible;               /* �����Ҧ�     1 bytes */
  unsigned int exmailbox;                /* �H�c�ʼ�     4 bytes */
  unsigned int exmailboxk;               /* �H�cK��      4 bytes */
  unsigned int toquery;                  /* �n�_��       4 bytes */
  unsigned int bequery;                  /* �H���       4 bytes */
  char toqid[IDLEN + 1];          /* �e���d��    13 bytes */
  char beqid[IDLEN + 1];          /* �e���Q�֬d  13 bytes */
  unsigned long int totaltime;    /* �W�u�`�ɼ�   8 bytes */
  unsigned int sendmsg;                  /* �o�T������   4 bytes */
  unsigned int receivemsg;               /* ���T������   4 bytes */
  unsigned long int goldmoney;    /* ���Ъ���     8 bytes */
  unsigned long int silvermoney;  /* �ȹ�         8 bytes */
  unsigned long int exp;          /* �g���       8 bytes */
  time_t dtime;                   /* �s�ڮɶ�     4 bytes */
  int scoretimes;                 /* ��������     4 bytes */
  unsigned char rtimes;                  /* ����U�榸�� 1 bytes */
  int award;                      /* ���g�P�_     4 bytes */
  int pagermode;                  /* �I�s������   4 bytes */
  char pagernum[7];               /* �I�s�����X   7 bytes */
  char feeling[5];                /* �߱�����     5 bytes */
  char title[20];                 /* �ٿ�(�ʸ�)  20 bytes */
  unsigned int five_win;
  unsigned int five_lost;
  unsigned int five_draw;
  time_t last_score_time;         /* �̫�@�������ɶ� 4 bytes */
  char pad[87];                  /* �ŵ۶񺡦�512��      */
};

typedef struct wduserec_t wduserec_t;
