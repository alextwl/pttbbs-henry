#define IDLEN      12
#define BTLEN      48             /* Length of board title */

/* TODO �ʺA��s����줣���Ӹ�n�g�J�ɮת��V�b�@�_,
 * �ܤ֥έ� struct �]�_�Ӥ��� */
typedef struct boardheader_t {
    char    brdname[IDLEN + 1];          /* bid */
    char    title[BTLEN + 1];
    char    BM[IDLEN * 3 + 3];           /* BMs' userid, token '/' */
    unsigned int    brdattr;             /* board���ݩ� */
    char    chesscountry;
    unsigned char   vote_limit_posts;    /* �s�p : �峹�g�ƤU�� */
    unsigned char   vote_limit_logins;   /* �s�p : �n�J���ƤU�� */
    unsigned char   vote_limit_regtime;  /* �s�p : ���U�ɶ����� */
    time_t bupdate;                     /* note update time */
    unsigned char   post_limit_posts;    /* �o��峹 : �峹�g�ƤU�� */
    unsigned char   post_limit_logins;   /* �o��峹 : �n�J���ƤU�� */
    unsigned char   post_limit_regtime;  /* �o��峹 : ���U�ɶ����� */
    unsigned char   bvote;               /* ���|�� Vote �� */
    time_t vtime;                       /* Vote close time */
    unsigned int    level;               /* �i�H�ݦ��O���v�� */
    time_t perm_reload;                 /* �̫�]�w�ݪO���ɶ� */
    int     gid;                         /* �ݪO���ݪ����O ID */
    int     next[2];	                 /* �b�P�@��gid�U�@�ӬݪO �ʺA����*/
    int     firstchild[2];	         /* �ݩ�o�ӬݪO���Ĥ@�Ӥl�ݪO */
    int     parent;
    int     childcount;                  /* ���h�֭�child */
    int     nuser;                       /* �h�֤H�b�o�O */
    int     postexpire;                  /* postexpire */
    time_t endgamble;
    char    posttype[33];
    char    posttype_f;
    unsigned char fastrecommend_pause;	/* �ֳt�s�����j */
    char    pad3[49];
} pttboardheader_t;

/* �U���O�K�i��� */
#define BRD_NOZAP       0000000001         /* ���izap  */
#define BRD_NOCOUNT     0000000002         /* ���C�J�έp */
#define BRD_NOTRAN      0000000004         /* ����H */
#define BRD_GROUPBOARD  0000000010         /* �s�ժO */
#define BRD_HIDE        0000000020         /* ���êO (�ݪO�n�ͤ~�i��) */
#define BRD_POSTMASK    0000000040         /* ����o��ξ\Ū */
#define BRD_ANONYMOUS   0000000100         /* �ΦW�O */
#define BRD_DEFAULTANONYMOUS 0000000200    /* �w�]�ΦW�O */
#define BRD_BAD		0000000400         /* �H�k��i���ݪO */
#define BRD_VOTEBOARD   0000001000         /* �s�p���ݪO */
#define BRD_WARNEL      0000002000         /* �s�p���ݪO */
#define BRD_TOP         0000004000         /* �����ݪO�s�� */
#define BRD_NORECOMMEND 0000010000         /* ���i���� */
#define BRD_BLOG        0000020000         /* BLOG */
#define BRD_BMCOUNT	0000040000	  /* �O�D�]�w�C�J�O�� */
#define BRD_SYMBOLIC	0000100000	  /* symbolic link to board */
#define BRD_NOBOO       0000200000         /* ���i�N */
#define BRD_LOCALSAVE   0000400000         /* �w�] Local Save */
#define BRD_RESTRICTEDPOST 0001000000      /* �O�ͤ~��o�� */
#define BRD_GUESTPOST   0002000000         /* guest�� post */
#define BRD_COOLDOWN    0004000000         /* �N�R */
#define BRD_CPLOG       0010000000         /* �۰ʯd����O�� */
#define BRD_NOFASTRECMD 0020000000         /* �T��ֳt���� */
#define BRD_IPLOGRECMD  0040000000         /* ����O�� IP */
#define BRD_OVER18      0100000000         /* �Q�K�T */

#define BRD_LINK_TARGET(x)	((x)->postexpire)
#define GROUPOP()               (currmode & MODE_GROUPOP)


/* --- WD --- */

/* ----------------------------------------------------- */
/* BOARDS struct : 512 bytes                             */
/* ----------------------------------------------------- */
#if 0
#define BRD_NOZAP       00001         /* ���izap  */
#define BRD_NOCOUNT     00002         /* ���C�J�έp */
#define BRD_NOTRAN      00004         /* ����H */
#define BRD_GROUPBOARD  00010         /* �s�ժO */
#define BRD_HIDE        00020         /* ���êO (�ݪO�n�ͤ~�i��) */
#define BRD_POSTMASK    00040         /* ����o��ξ\Ū */
#define BRD_ANONYMOUS   00100         /* �ΦW�O? */
#define BRD_CLASS       00200         /* �����ݪO */
#define BRD_GOOD        00400         /* �u�}�ݪO */
#define BRD_PERSONAL    01000         /* �ӤH�ݪO */
#define BRD_NOFOWARD    02000         /* �T����� */
#define BRD_NOSCORE     00010000        /* �������� */
#endif
#define PASSLEN  14             /* Length of encrypted passwd field */
struct boardheader
{
  char brdname[IDLEN + 1];      /* �ݪO�^��W��    13 bytes */
  char title[BTLEN + 1];        /* �ݪO����W��    49 bytes */
  char BM[IDLEN * 3 + 3];       /* �O�DID�M"/"     39 bytes */
  unsigned int brdattr;                /* �ݪO���ݩ�       4 bytes */
  time_t bupdate;               /* note update time 4 bytes */
  unsigned char bvote;                 /* Vote flags       1 bytes */
  time_t vtime;                 /* Vote close time  4 bytes */
  unsigned int level;                  /* �i�H�ݦ��O���v�� 4 bytes */
  unsigned long int totalvisit; /* �`���X�H��       8 bytes */
  unsigned long int totaltime;  /* �`���d�ɶ�       8 bytes */
  char lastvisit[IDLEN + 1];    /* �̫�ݸӪO���H  13 bytes */
  time_t opentime;              /* �}�O�ɶ�         4 bytes */
  time_t lastime;               /* �̫���X�ɶ�     4 bytes */
  char passwd[PASSLEN];         /* �K�X            14 bytes */
  unsigned long int postotal;   /* �`���q :p        8 bytes */
// wildcat note : check �o�� , expire.conf �����L�C�C�����a ...
  unsigned int maxpost;                /* �峹�W��         4 bytes */
  unsigned int maxtime;                /* �峹�O�d�ɶ�     4 bytes */
  char desc[3][80];             /* ����y�z       240 bytes */
//  uschar bgamble;               /* Gamble flags       1 bytes */
//  time_t gtime;                 /* Gamble close time  4 bytes */
  char pad[87];
};
typedef struct boardheader wdboardheader_t;
