#ifndef _SUCK_SUCK_H
#define _SUCK_SUCK_H 1

#include <stdio.h>	/* for FILE */
#include "suck_config.h"

/* link list structure one for each article */
typedef struct LinkList {
	struct LinkList *next;
	char msgnr[MAX_MSGID_LEN];
	int groupnr;
	long nr;
	long dbnr;
	char mandatory;
	char downloaded;
	char delete;
	char sentcmd;
} List, *PList;

/* link list for group names */
typedef struct GroupList {
	char group[MAX_GRP_LEN];
	int nr;
	struct GroupList *next;
} Groups, *PGroups;

/* link list for list overview.fmt */
typedef struct XOVERVIEW {
	struct XOVERVIEW *next;
	char *header;  /* dynamically alloced */
	char *field;
	int fieldlen;
	int full;
} Overview, *POverview;

/* Master Structure */
typedef struct {
	PList head;
	PList curr;
	int nritems;
	int nrgot;
	int sockfd;
	int MultiFile;
	int status_file;
	int do_killfile;
	int do_chkhistory;
	int do_modereader;
	int always_batch;
	int cleanup;
	int batch;
	int pause_time;
	int pause_nrmsgs;
	int sig_pause_time;
	int sig_pause_nrmsgs;
	int killfile_log;
	int debug;
	int rescan;
	int quiet;
	int kill_ignore_postfix;
	int reconnect_nr;
	int do_active;
	int nrmode;
	int auto_auth;
	int no_dedupe;
	int chk_msgid;
	int prebatch;
	int skip_on_restart;
	int use_gui;
	int do_xover;
	int conn_dedupe;
	int conn_active;
	int header_only;
	int active_lastread;
	int use_xover;
	int resetcounter;
	int low_read;
	int show_group;
	unsigned short int portnr;
	long rnews_size;
	FILE *msgs;
	FILE *innfeed;
	int db;
	const char *userid;
	const char *passwd;
	const char *host;
	const char *batchfile;
	const char *status_file_name;
	const char *phrases;
	const char *errlog;
	const char *localhost;
	const char *activefile;
	const char *kill_log_name;
	const char *post_filter;
	const char *history_file;
	PGroups groups;
	int grpnr;
	void *killp;
	void *xoverp;
	POverview xoverview;
} Master, *PMaster;

int get_a_chunk(int, FILE *);
void free_one_node(PList);
int send_command(PMaster, const char *, char **, int);
int get_message_index(PMaster);
int do_one_group(PMaster, char *, char *, FILE *, long, int);
const char *build_command(PMaster, const char *, PList);

int allocnode(PMaster, char *, int, char *, long);
int do_connect(PMaster, int);

#ifdef MYSIGNAL
#endif

enum { RETVAL_ERROR = -1, RETVAL_OK = 0, RETVAL_NOARTICLES, RETVAL_UNEXPECTEDANS, RETVAL_VERNR, \
       RETVAL_NOAUTH, RETVAL_EMPTYKILL, RETVAL_NOXOVER };
enum { BATCH_FALSE, BATCH_INNXMIT, BATCH_RNEWS, BATCH_LMOVE, BATCH_INNFEED, BATCH_LIHAVE };		/* poss values for batch variable */
enum { MANDATORY_YES = 'M' , MANDATORY_OPTIONAL = 'O'};	/* do we have to download this article */
/* note the MANDATORY_DELETE is used in the dedupe and checkhistory phases to flag em for deletion */
enum {CONNECT_FIRST, CONNECT_AGAIN};

#endif /* _SUCK_SUCK_H */
