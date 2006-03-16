#define IDLEN      12
#define PASSLEN    14             /* Length of encrypted passwd field */
#define REGLEN     38             /* Length of registration data */

#define PASSWD_VERSION	2275
#define time4_t time_t

typedef struct chicken_t {
    char    name[20];
    char    type;             /* 物種 */
    unsigned char   tech[16]; /* 技能 */
    time4_t birthday;         /* 生日 */
    time4_t lastvisit;        /* 上次照顧時間 */
    int     oo;               /* 補品 */
    int     food;             /* 食物 */
    int     medicine;         /* 藥品 */
    int     weight;           /* 體重 */
    int     clean;            /* 乾淨 */
    int     run;              /* 敏捷度 */
    int     attack;           /* 攻擊力 */
    int     book;             /* 知識 */
    int     happy;            /* 快樂 */
    int     satis;            /* 滿意度 */
    int     temperament;      /* 氣質 */
    int     tiredstrong;      /* 疲勞度 */
    int     sick;             /* 病氣指數 */
    int     hp;               /* 血量 */
    int     hp_max;           /* 滿血量 */
    int     mm;               /* 法力 */
    int     mm_max;           /* 滿法力 */
    time4_t cbirth;           /* 實際計算用的生日 */
    int     pad[2];           /* 留著以後用 */
} chicken_t;

typedef struct pttuserec_t {
    unsigned int    version;	/* version number of this sturcture, we
    				 * use revision number of project to denote.*/

    char    userid[IDLEN + 1];	/* ID */
    char    realname[20];	/* 真實姓名 */
    char    nickname[24];	/* 暱稱 */
    char    passwd[PASSLEN];	/* 密碼 */
    unsigned int    uflag;	/* 習慣1 */
    unsigned int    uflag2;	/* 習慣2 */
    unsigned int    userlevel;	/* 權限 */
    unsigned int    numlogins;	/* 上站次數 */
    unsigned int    numposts;	/* 文章篇數 */
    time4_t firstlogin;		/* 註冊時間 */
    time4_t lastlogin;		/* 最近上站時間 */
    char    lasthost[16];	/* 上次上站來源 */
    int     money;		/* Ptt幣 */
    char    remoteuser[3];	/* 保留 目前沒用到的 */
    char    proverb;		/* 座右銘 (unused) */
    char    email[50];		/* Email */
    char    address[50];	/* 住址 */
    char    justify[REGLEN + 1];    /* 審核資料 */
    unsigned char   month;	/* 生日 月 */
    unsigned char   day;	/* 生日 日 */
    unsigned char   year;	/* 生日 年 */
    unsigned char   sex;	/* 性別 */
    unsigned char   state;	/* TODO unknown (unused ?) */
    unsigned char   pager;	/* 呼叫器狀態 */
    unsigned char   invisible;	/* 隱形狀態 */
    unsigned int    exmailbox;	/* 購買信箱數 TODO short 就夠了 */
    chicken_t       mychicken;	/* 寵物 */
    time4_t lastsong;		/* 上次點歌時間 */
    unsigned int    loginview;	/* 進站畫面 */
    unsigned char   channel;	/* TODO unused */
    unsigned short  vl_count;	/* 違法記錄 ViolateLaw counter */
    unsigned short  five_win;	/* 五子棋戰績 勝 */
    unsigned short  five_lose;	/* 五子棋戰績 敗 */
    unsigned short  five_tie;	/* 五子棋戰績 和 */
    unsigned short  chc_win;	/* 象棋戰績 勝 */
    unsigned short  chc_lose;	/* 象棋戰績 敗 */
    unsigned short  chc_tie;	/* 象棋戰績 和 */
    int     mobile;		/* 手機號碼 */
    char    mind[4];		/* 心情 not a null-terminate string */
    char    pad0[11];		/* 從前放 ident 身份證字號，現在可以拿來做別的事了，
				   不過最好記得要先清成 0 */
    unsigned char   signature;	/* 慣用簽名檔 */

    unsigned char   goodpost;	/* 評價為好文章數 */
    unsigned char   badpost;	/* 評價為壞文章數 */
    unsigned char   goodsale;	/* 競標 好的評價  */
    unsigned char   badsale;	/* 競標 壞的評價  */
    char    myangel[IDLEN+1];	/* 我的小天使 */
    unsigned short  chess_elo_rating;	/* 象棋等級分 */
    unsigned int    withme;	/* 我想找人下棋，聊天.... */
    time4_t timeremovebadpost;  /* 上次刪除劣文時間 */
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
  char userid[IDLEN + 1];         /* 使用者名稱  13 bytes */
  char realname[20];              /* 真實姓名    20 bytes */
  char username[24];              /* 暱稱        24 bytes */
  char passwd[PASSLEN];           /* 密碼        14 bytes */
  unsigned char uflag;                   /* 使用者選項   1 byte  */
  unsigned int userlevel;                /* 使用者權限   4 bytes */
  ushort numlogins;               /* 上站次數     2 bytes */
  ushort numposts;                /* POST次數     2 bytes */
  time_t firstlogin;              /* 註冊時間     4 bytes */
  time_t lastlogin;               /* 前次上站     4 bytes */
  char lasthost[24];              /* 上站地點    24 bytes */
  char vhost[24];                 /* 虛擬網址    24 bytes */
  char email[50];                 /* E-MAIL      50 bytes */
  char address[50];               /* 地址        50 bytes */
  char justify[REGLEN + 1];       /* 註冊資料    39 bytes */
  unsigned char month;                   /* 出生月份     1 byte  */
  unsigned char day;                     /* 出生日期     1 byte  */
  unsigned char year;                    /* 出生年份     1 byte  */
  unsigned char sex;                     /* 性別         1 byte  */
  unsigned char state;                   /* 狀態??       1 byte  */
  unsigned int habit;                    /* 喜好設定     4 bytes */
  unsigned char pager;                   /* 心情顏色     1 bytes */
  unsigned char invisible;               /* 隱身模式     1 bytes */
  unsigned int exmailbox;                /* 信箱封數     4 bytes */
  unsigned int exmailboxk;               /* 信箱K數      4 bytes */
  unsigned int toquery;                  /* 好奇度       4 bytes */
  unsigned int bequery;                  /* 人氣度       4 bytes */
  char toqid[IDLEN + 1];          /* 前次查誰    13 bytes */
  char beqid[IDLEN + 1];          /* 前次被誰查  13 bytes */
  unsigned long int totaltime;    /* 上線總時數   8 bytes */
  unsigned int sendmsg;                  /* 發訊息次數   4 bytes */
  unsigned int receivemsg;               /* 收訊息次數   4 bytes */
  unsigned long int goldmoney;    /* 風塵金幣     8 bytes */
  unsigned long int silvermoney;  /* 銀幣         8 bytes */
  unsigned long int exp;          /* 經驗值       8 bytes */
  time_t dtime;                   /* 存款時間     4 bytes */
  int scoretimes;                 /* 評分次數     4 bytes */
  unsigned char rtimes;                  /* 填註冊單次數 1 bytes */
  int award;                      /* 獎懲判斷     4 bytes */
  int pagermode;                  /* 呼叫器門號   4 bytes */
  char pagernum[7];               /* 呼叫器號碼   7 bytes */
  char feeling[5];                /* 心情指數     5 bytes */
  char title[20];                 /* 稱謂(封號)  20 bytes */
  unsigned int five_win;
  unsigned int five_lost;
  unsigned int five_draw;
  time_t last_score_time;         /* 最後一次評分時間 4 bytes */
  char pad[87];                  /* 空著填滿至512用      */
};

typedef struct wduserec_t wduserec_t;
