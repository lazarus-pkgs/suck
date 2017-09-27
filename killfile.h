#ifndef _SUCK_KILLFILE_H
#define _SUCK_KILLFILE_H 1

#define SIZEOF_SKIPARRAY 256

#ifdef HAVE_UNISTD_H
#include <sys/types.h> /* for pid_t */
#endif

#ifdef PERL_EMBED
#include <EXTERN.h>
#include <perl.h>
#endif

#ifdef HAVE_REGEX_H
#include <regex.h>	/* for regex_t */
#endif
typedef struct MYREGEX {
#ifdef HAVE_REGEX_H
	regex_t *ptrs;
#endif
	char *header;
	char *string;
	struct MYREGEX *next;
/*	void *next; */
	unsigned char skiparray[SIZEOF_SKIPARRAY];
	int case_sensitive;
} my_regex, *pmy_regex;

/* this is a structure so can add other kill options later */
typedef struct {
	int hilines;	/* nr of lines max article length */
	int lowlines;	/* nr of lines min article length */
	int maxgrps;	/* max nr of grps (to prevent spams) */
	int maxxref;    /* max nr of Xrefs (another spamer) */
	unsigned long bodybig;	/* max size of article body  */
	unsigned long bodysmall; /* minimum size of article body */
	char quote;	/* character to use as quote (for case compare) */
	char non_regex;	/* character to use as non_regex (don't do regex compare ) */
#ifdef HAVE_REGEX_H	
	int use_extended;	/* do we use extended regex */
#endif
	pmy_regex header;       /* scan the entire header */
	pmy_regex body;		/* scan the body */
	pmy_regex list;	/* linked list of headers and what to match */
} OneKill, *POneKill;

typedef struct {
	OneKill match;
	int delkeep;
	char *group;	/* dynamically allocated */
} Group, *PGroup;

typedef struct {
	int Stdin;
	int Stdout;
	pid_t Pid;
} Child;

typedef struct killstruct {
	FILE *logfp;
	int logyn;
	int grp_override;
	int tie_delete;
	int totgrps;
	int ignore_postfix;
	int xover_log_long;
	PGroup grps;	/* dynamicly allocated array */
	int ( *killfunc)(PMaster, struct killstruct *, char *, int); /*function to call to check header */
	char *pbody;
	unsigned long bodylen;
	int use_extended_regex;
	Child child;		/* these two are last since can't initialize */
	OneKill master;
#ifdef PERL_EMBED
	PerlInterpreter *perl_int;
#endif
} KillStruct, *PKillStruct;

/* function prototypes for killfile.c  and xover.c*/
int get_one_article_kill(PMaster, int, long);
PKillStruct parse_killfile(int, int, int, int);
void free_killfile(PKillStruct);
int chk_msg_kill(PMaster, PKillStruct, char *, int);
int regex_block(char *, pmy_regex, int);

/* function prototypes for killprg.c */
int killprg_forkit(PKillStruct, char *, int, int);
int chk_msg_kill_fork(PMaster, PKillStruct, char *, int);
void killprg_closeit(PKillStruct);
void killprg_sendoverview(PMaster);
int killprg_sendxover(PMaster, char *);
#ifdef PERL_EMBED
int killperl_setup(PKillStruct, char *, int, int);
int chk_msg_kill_perl(PMaster, PKillStruct, char *, int);
void killperl_done(PKillStruct);
int killperl_sendxover(PMaster, char *);
#endif

enum { KILL_XOVER, KILL_KILLFILE}; /* are we doing xover or killfile */
enum { KILL_LOG_NONE, KILL_LOG_SHORT, KILL_LOG_LONG };  /* what level of logging do we use */
enum { DELKEEP_KEEP, DELKEEP_DELETE }; /* is this a keep or delete group */

#define COMMA ','			/* group separator in newsgroup line in article */
#define SPACE ' '                       /* another group separator in newsgroup/xref line in article */
#define COLON ':'                       /* group/article nr separator in Xref line */

#endif /* _SUCK_KILLFILE_H */
