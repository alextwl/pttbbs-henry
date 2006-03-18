#define IDLEN      12
#define TTLEN      64             /* Length of title */
#define FNLEN      28             /* Length of filename */

typedef struct fileheader_t {
    char    filename[FNLEN];         /* M.1120582370.A.1EA [19+1] */
    int	    textlen;		     /* main text length in post */
    char    pad;		     /* padding, not used */
    char    recommend;               /* important level */
    char    owner[IDLEN + 2];        /* uid[.] */
    char    date[6];                 /* [02/02] or space(5) */
    char    title[TTLEN + 1];
    /* TODO this multi is a mess now. */
    union {
	/* TODO: MOVE money to outside multi!!!!!! */
	int money;
	int anon_uid;
	/* different order to match alignment */
	struct {
	    unsigned char posts;
	    unsigned char logins;
	    unsigned char regtime;
	    unsigned char pad[1];
	} vote_limits;
	struct {
	    /* is this ordering correct? */
	    unsigned int ref:31;
	    unsigned int flag:1;
	} refer;
    }	    multi;		    /* rocker: if bit32 on ==> reference */
    /* XXX dirty, split into flag and money if money of each file is less than 16bit? */
    unsigned char   filemode;        /* must be last field @ boards.c */
} pttfileheader_t;

#define FILE_LOCAL      0x1     /* local saved,  non-mail */
#define FILE_READ       0x1     /* already read, mail only */
#define FILE_MARKED     0x2     /* non-mail + mail */
#define FILE_DIGEST     0x4     /* digest,       non-mail */
#define FILE_REPLIED    0x4     /* replied,      mail only */
#define FILE_BOTTOM     0x8     /* push_bottom,  non-mail */
#define FILE_MULTI      0x8     /* multi send,   mail only */
#define FILE_SOLVED     0x10    /* problem solved, sysop/BM non-mail only */
#define FILE_HIDE       0x20    /* hide,	in announce */
#define FILE_BID        0x20    /* bid,		in non-announce */
#define FILE_BM         0x40    /* BM only,	in announce */
#define FILE_VOTE       0x40    /* for vote,	in non-announce */
#define FILE_ANONYMOUS  0x80    /* anonymous file */

#define STRLEN     80             /* Length of most string data */

/* --- WD --- */
/* ----------------------------------------------------- */
/* DIR of board struct : 128 bytes                       */
/* ----------------------------------------------------- */

struct wdfileheader
{
  char filename[33-1];         /* M.9876543210.A     32 bytes*/
  char score;                   /* score                1 bytes*/
  char savemode;                /* file save mode        1 bytes*/
  char owner[IDLEN + 2];        /* uid[.]               14 bytes*/
  char date[6];                 /* [02/02] or space(5)   6 bytes*/
  char title[72 + 1];        /* title                73 bytes*/
//  time_t chrono;                /* timestamp */
//  char dummy;
  unsigned char filemode;              /* must be last field @ boards.c 1 bytes*/
};
typedef struct wdfileheader wdfileheader_t;
