#define IDLEN      12
#define BTLEN      48             /* Length of board title */

/* TODO 動態更新的欄位不應該跟要寫入檔案的混在一起,
 * 至少用個 struct 包起來之類 */
typedef struct boardheader_t {
    char    brdname[IDLEN + 1];          /* bid */
    char    title[BTLEN + 1];
    char    BM[IDLEN * 3 + 3];           /* BMs' userid, token '/' */
    unsigned int    brdattr;             /* board的屬性 */
    char    chesscountry;
    unsigned char   vote_limit_posts;    /* 連署 : 文章篇數下限 */
    unsigned char   vote_limit_logins;   /* 連署 : 登入次數下限 */
    unsigned char   vote_limit_regtime;  /* 連署 : 註冊時間限制 */
    time_t bupdate;                     /* note update time */
    unsigned char   post_limit_posts;    /* 發表文章 : 文章篇數下限 */
    unsigned char   post_limit_logins;   /* 發表文章 : 登入次數下限 */
    unsigned char   post_limit_regtime;  /* 發表文章 : 註冊時間限制 */
    unsigned char   bvote;               /* 正舉辦 Vote 數 */
    time_t vtime;                       /* Vote close time */
    unsigned int    level;               /* 可以看此板的權限 */
    time_t perm_reload;                 /* 最後設定看板的時間 */
    int     gid;                         /* 看板所屬的類別 ID */
    int     next[2];	                 /* 在同一個gid下一個看板 動態產生*/
    int     firstchild[2];	         /* 屬於這個看板的第一個子看板 */
    int     parent;
    int     childcount;                  /* 有多少個child */
    int     nuser;                       /* 多少人在這板 */
    int     postexpire;                  /* postexpire */
    time_t endgamble;
    char    posttype[33];
    char    posttype_f;
    unsigned char fastrecommend_pause;	/* 快速連推間隔 */
    char    pad3[49];
} pttboardheader_t;

/* 下面是八進位喔 */
#define BRD_NOZAP       0000000001         /* 不可zap  */
#define BRD_NOCOUNT     0000000002         /* 不列入統計 */
#define BRD_NOTRAN      0000000004         /* 不轉信 */
#define BRD_GROUPBOARD  0000000010         /* 群組板 */
#define BRD_HIDE        0000000020         /* 隱藏板 (看板好友才可看) */
#define BRD_POSTMASK    0000000040         /* 限制發表或閱讀 */
#define BRD_ANONYMOUS   0000000100         /* 匿名板 */
#define BRD_DEFAULTANONYMOUS 0000000200    /* 預設匿名板 */
#define BRD_BAD		0000000400         /* 違法改進中看板 */
#define BRD_VOTEBOARD   0000001000         /* 連署機看板 */
#define BRD_WARNEL      0000002000         /* 連署機看板 */
#define BRD_TOP         0000004000         /* 熱門看板群組 */
#define BRD_NORECOMMEND 0000010000         /* 不可推薦 */
#define BRD_BLOG        0000020000         /* BLOG */
#define BRD_BMCOUNT	0000040000	  /* 板主設定列入記錄 */
#define BRD_SYMBOLIC	0000100000	  /* symbolic link to board */
#define BRD_NOBOO       0000200000         /* 不可噓 */
#define BRD_LOCALSAVE   0000400000         /* 預設 Local Save */
#define BRD_RESTRICTEDPOST 0001000000      /* 板友才能發文 */
#define BRD_GUESTPOST   0002000000         /* guest能 post */
#define BRD_COOLDOWN    0004000000         /* 冷靜 */
#define BRD_CPLOG       0010000000         /* 自動留轉錄記錄 */
#define BRD_NOFASTRECMD 0020000000         /* 禁止快速推文 */
#define BRD_IPLOGRECMD  0040000000         /* 推文記錄 IP */
#define BRD_OVER18      0100000000         /* 十八禁 */

#define BRD_LINK_TARGET(x)	((x)->postexpire)
#define GROUPOP()               (currmode & MODE_GROUPOP)


/* --- WD --- */

/* ----------------------------------------------------- */
/* BOARDS struct : 512 bytes                             */
/* ----------------------------------------------------- */
#if 0
#define BRD_NOZAP       00001         /* 不可zap  */
#define BRD_NOCOUNT     00002         /* 不列入統計 */
#define BRD_NOTRAN      00004         /* 不轉信 */
#define BRD_GROUPBOARD  00010         /* 群組板 */
#define BRD_HIDE        00020         /* 隱藏板 (看板好友才可看) */
#define BRD_POSTMASK    00040         /* 限制發表或閱讀 */
#define BRD_ANONYMOUS   00100         /* 匿名板? */
#define BRD_CLASS       00200         /* 分類看板 */
#define BRD_GOOD        00400         /* 優良看板 */
#define BRD_PERSONAL    01000         /* 個人看板 */
#define BRD_NOFOWARD    02000         /* 禁止轉錄 */
#define BRD_NOSCORE     00010000        /* 評分停用 */
#endif
#define PASSLEN  14             /* Length of encrypted passwd field */
struct boardheader
{
  char brdname[IDLEN + 1];      /* 看板英文名稱    13 bytes */
  char title[BTLEN + 1];        /* 看板中文名稱    49 bytes */
  char BM[IDLEN * 3 + 3];       /* 板主ID和"/"     39 bytes */
  unsigned int brdattr;                /* 看板的屬性       4 bytes */
  time_t bupdate;               /* note update time 4 bytes */
  unsigned char bvote;                 /* Vote flags       1 bytes */
  time_t vtime;                 /* Vote close time  4 bytes */
  unsigned int level;                  /* 可以看此板的權限 4 bytes */
  unsigned long int totalvisit; /* 總拜訪人數       8 bytes */
  unsigned long int totaltime;  /* 總停留時間       8 bytes */
  char lastvisit[IDLEN + 1];    /* 最後看該板的人  13 bytes */
  time_t opentime;              /* 開板時間         4 bytes */
  time_t lastime;               /* 最後拜訪時間     4 bytes */
  char passwd[PASSLEN];         /* 密碼            14 bytes */
  unsigned long int postotal;   /* 總水量 :p        8 bytes */
// wildcat note : check 這裡 , expire.conf 的讓他慢慢消失吧 ...
  unsigned int maxpost;                /* 文章上限         4 bytes */
  unsigned int maxtime;                /* 文章保留時間     4 bytes */
  char desc[3][80];             /* 中文描述       240 bytes */
//  uschar bgamble;               /* Gamble flags       1 bytes */
//  time_t gtime;                 /* Gamble close time  4 bytes */
  char pad[87];
};
typedef struct boardheader wdboardheader_t;
