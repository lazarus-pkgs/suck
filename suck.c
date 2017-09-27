#include <config.h>

#include <stdio.h>
#include <stdlib.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#ifdef HAVE_DIRENT_H
# include <dirent.h>
#else
# ifdef HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# ifdef HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
#endif

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#include <string.h>
#include <sys/types.h>
#include <netdb.h>
#include <sys/stat.h>
#include <math.h>
#include <errno.h>
#include <ctype.h>

#ifdef DMALLOC
#include <dmalloc.h>
#endif

#include "suck_config.h"
#include "both.h"
#include "suck.h"
#include "suckutils.h"
#include "dedupe.h"
#include "phrases.h"
#include "killfile.h"
#include "timer.h"
#include "active.h"
#include "batch.h"
#include "xover.h"
#include "db.h"
#include "queue.h"

#ifdef MYSIGNAL 
#include <signal.h>
#endif

#ifdef CHECK_HISTORY
#include "chkhistory.h"
#endif

/* function prototypes */
int get_articles(PMaster);
int get_one_article(PMaster, int, long);
int do_supplemental(PMaster);
int restart_yn(PMaster);
int scan_args(PMaster, int, char *[]);
void do_cleanup(PMaster);
int do_authenticate(PMaster);
void load_phrases(PMaster);
void free_phrases(void);
int parse_args(PMaster, int, char *[]);
int get_group_number(PMaster, char *);
int do_supplemental(PMaster);
int do_sup_bynr(PMaster, char *);
int do_nodownload(PMaster);
#ifdef MYSIGNAL
RETSIGTYPE sighandler(int);
void pause_signal(int, PMaster);
enum {PAUSE_SETUP, PAUSE_DO};

/*------------------------------------------*/
int GotSignal = FALSE;		
/* the only static variable allowed, it's   */
/* the only graceful way to handle a signal */
/*------------------------------------------*/
#endif

/* set up for phrases */
char **both_phrases=default_both_phrases;
char **suck_phrases=default_suck_phrases;
char **timer_phrases=default_timer_phrases;
char **chkh_phrases=default_chkh_phrases;
char **dedupe_phrases=default_dedupe_phrases;
char **killf_reasons=default_killf_reasons;
char **killf_phrases=default_killf_phrases;
char **killp_phrases=default_killp_phrases;
char **sucku_phrases=default_sucku_phrases;
char **active_phrases=default_active_phrases;
char **batch_phrases=default_batch_phrases;
char **xover_phrases=default_xover_phrases;
char **xover_reasons=default_xover_reasons;
char **queue_phrases=default_queue_phrases;

enum { STATUS_STDOUT, STATUS_STDERR };
enum { RESTART_YES, RESTART_NO, RESTART_ERROR };

enum {
	ARG_NO_MATCH, ARG_ALWAYS_BATCH, ARG_BATCH_INN, ARG_BATCH_RNEWS, ARG_BATCH_LMOVE, \
	ARG_BATCH_INNFEED, ARG_BATCH_POST, ARG_CLEANUP, ARG_DIR_TEMP, ARG_DIR_DATA, ARG_DIR_MSGS, \
	ARG_DEF_ERRLOG, ARG_HOST, ARG_NO_POSTFIX, ARG_LANGUAGE, ARG_MULTIFILE,  ARG_POSTFIX, \
	ARG_QUIET, ARG_RNEWSSIZE, ARG_DEF_STATLOG, ARG_WAIT_SIG, ARG_ACTIVE, ARG_RECONNECT, \
	ARG_DEBUG, ARG_ERRLOG, ARG_HISTORY, ARG_KILLFILE, ARG_KLOG_NONE, ARG_KLOG_SHORT,  \
	ARG_KLOG_LONG, ARG_MODEREADER, ARG_PORTNR, ARG_PASSWD, ARG_RESCAN, ARG_STATLOG, \
	ARG_USERID, ARG_VERSION, ARG_WAIT, ARG_LOCALHOST, ARG_TIMEOUT, ARG_NRMODE, ARG_AUTOAUTH, \
	ARG_NODEDUPE, ARG_NO_CHK_MSGID, ARG_READACTIVE, ARG_PREBATCH, ARG_SKIP_ON_RESTART, \
	ARG_KLOG_NAME, ARG_USEGUI, ARG_XOVER, ARG_CONN_DEDUPE, ARG_POST_FILTER, ARG_CONN_ACTIVE, \
	ARG_HIST_FILE, ARG_HEADER_ONLY, ARG_ACTIVE_LASTREAD, ARG_USEXOVER, ARG_RESETCOUNTER, \
	ARG_LOW_READ, ARG_SHOW_GROUP
}; 

typedef struct Arglist{
	const char *sarg;
	const char *larg;
	int nr_params;
	int flag;
	int errmsg;		/* this is an index into suck_phrases */
} Args, *Pargs;

const Args arglist[] = {
	{"a",  "always_batch", 0, ARG_ALWAYS_BATCH, -1},
	{"bi", "batch_inn",    1, ARG_BATCH_INN, 40},
	{"br", "batch_rnews",  1, ARG_BATCH_RNEWS, 40},
	{"bl", "batch_lmove",  1, ARG_BATCH_LMOVE, 40},
	{"bf", "batch_innfeed",1, ARG_BATCH_INNFEED, 40},
	{"bp", "batch_post",   0, ARG_BATCH_POST, -1},
	{"c",  "cleanup",      0, ARG_CLEANUP, -1},
	{"dt", "dir_temp",    1, ARG_DIR_TEMP, 37},
	{"dd", "dir_data",    1, ARG_DIR_DATA, 37},
	{"dm", "dir_msgs",    1, ARG_DIR_MSGS, 37},
	{"e",  "def_error_log", 0, ARG_DEF_ERRLOG, -1},
	{"f",  "reconnect_dedupe", 0, ARG_CONN_DEDUPE, -1},
	{"g",  "header_only", 0, ARG_HEADER_ONLY, -1},	
	{"h",  "host", 1, ARG_HOST, 51},
	{"hl", "localhost", 1, ARG_LOCALHOST, 50},
	{"i",  "default_activeread", 1, ARG_ACTIVE_LASTREAD, 65},	
	{"k",  "kill_no_postfix", 0, ARG_NO_POSTFIX, -1},
	{"l",  "language_file", 1, ARG_LANGUAGE, 47},
	{"lr", "low_read", 0, ARG_LOW_READ, -1},	
	{"m",  "multifile", 0, ARG_MULTIFILE, -1},
	{"n",  "number_mode", 0, ARG_NRMODE, -1},
	{"p",  "postfix", 1, ARG_POSTFIX, 36},
	{"q",  "quiet", 0, ARG_QUIET, -1},
	{"r",  "rnews_size", 1, ARG_RNEWSSIZE, 35},
	{"rc", "reset_counter", 0, ARG_RESETCOUNTER, -1},	
	{"s",  "def_status_log", 0, ARG_DEF_STATLOG, -1},
	{"sg", "show_group", 0, ARG_SHOW_GROUP, -1},	
	{"u",  "auto_authorization", 0, ARG_AUTOAUTH, -1},
	{"w",  "wait_signal", 2, ARG_WAIT_SIG, 46},
	{"x",  "no_chk_msgid", 0, ARG_NO_CHK_MSGID, -1},
	{"y",  "post_filter", 1, ARG_POST_FILTER, 62},	
	{"z",  "no_dedupe", 0, ARG_NODEDUPE, -1},	
	{"A",  "active", 0, ARG_ACTIVE, -1},
	{"AL", "read_active", 1, ARG_READACTIVE, 56},
	{"B",  "pre-batch", 0, ARG_PREBATCH, -1},	
	{"C",  "reconnect", 1, ARG_RECONNECT, 49},
	{"D",  "debug", 0, ARG_DEBUG, -1},
	{"E",  "error_log", 1, ARG_ERRLOG, 41},
	{"F",  "reconnect_active", 0, ARG_CONN_ACTIVE, -1},
	{"G",  "use_gui", 0, ARG_USEGUI, -1},
	{"H",  "no_history", 0, ARG_HISTORY, -1},
	{"HF", "history_file", 1, ARG_HIST_FILE, 64},
	{"K",  "killfile", 0, ARG_KILLFILE, -1},
	{"L",  "kill_log_none", 0, ARG_KLOG_NONE, -1},
	{"LF", "kill_log_name", 1, ARG_KLOG_NAME, 61},
	{"LS", "kill_log_short", 0, ARG_KLOG_SHORT, -1},
	{"LL", "kill_log_long",  0, ARG_KLOG_LONG, -1},
	{"M",  "mode_reader", 0, ARG_MODEREADER, -1},
	{"N",  "portnr", 1, ARG_PORTNR, 45},
	{"O",  "skip_on_restart", 0, ARG_SKIP_ON_RESTART, -1},	
	{"P",  "password", 1, ARG_PASSWD, 44},
	{"R",  "no_rescan", 0, ARG_RESCAN, -1},
	{"S",  "status_log", 1, ARG_STATLOG, 42},
#ifdef TIMEOUT
	{"T",  "timeout", 1, ARG_TIMEOUT, 52},
#endif	
	{"U",  "userid",  1, ARG_USERID,  43},
	{"V",  "version", 0, ARG_VERSION, -1},
	{"W",  "wait", 2, ARG_WAIT, 46},
	{"X",  "no_xover", 0, ARG_XOVER, -1},
	{"Z",  "use_xover", 0, ARG_USEXOVER, -1},
	
};

#define MAX_ARG_PARAMS 2	/* max nr of params with any arg */
#define NR_ARGS (sizeof(arglist)/sizeof(arglist[0]))

/*------------------------------------------------------------------*/
int main(int argc, char *argv[]) {
	
	struct stat sbuf;
	Master master;
	PList temp;
	PGroups ptemp;
	POverview pov;
	char *inbuf, **args, **fargs = NULL;
	int nr, resp, loop, fargc, retval = RETVAL_OK;	

#ifdef LOCKFILE
	const char *lockfile = NULL;
#endif
#ifdef MYSIGNAL
#ifdef HAVE_SIGACTION
	struct sigaction sigs;
#endif
#endif	
	
	/* initialize master structure */
	master.head = master.curr = NULL;
	master.nritems = master.nrgot = 0;
	master.MultiFile = FALSE;
	master.msgs = stdout;
	master.sockfd = -1;
	master.status_file = FALSE;	/* are status messages going to a file */
	master.status_file_name = NULL;
	master.do_killfile = TRUE;
	master.do_chkhistory = TRUE;
	master.do_modereader = FALSE;
	master.always_batch = FALSE;
	master.rnews_size = 0L;
	master.batch = BATCH_FALSE;
	master.batchfile = NULL;
	master.cleanup = FALSE;
	master.portnr = DEFAULT_NNRP_PORT;
	master.host = getenv("NNTPSERVER");		/* the default */
	master.pause_time = -1;
	master.pause_nrmsgs = -1;
	master.sig_pause_time = -1;
	master.sig_pause_nrmsgs = -1;
	master.killfile_log = KILL_LOG_LONG;	/* do we log killed messages */
	master.phrases = NULL;
	master.errlog = NULL;
	master.debug = FALSE;
	master.rescan = TRUE;		/* do we do rescan on a restart */
	master.quiet = FALSE;		/* do we display BPS and msg count */
	master.killp = NULL;		/* pointer to killfile structure */
	master.kill_ignore_postfix = FALSE;
	master.reconnect_nr = 0;	/* how many x msgs do we disconnect and reconnect 0 = never */
	master.innfeed = NULL;
	master.do_active = FALSE; /* do we scan the local active list  */
	master.localhost = NULL;
	master.groups = NULL;
	master.nrmode = FALSE;    /* use nrs or msgids to get article */
	master.auto_auth = FALSE; /* do we do auto authorization */
	master.passwd = NULL;
	master.userid = NULL;
	master.no_dedupe = FALSE;
	master.chk_msgid = TRUE; /* do we check MsgID for trailing > */
	master.activefile = NULL;
	master.prebatch = FALSE; /* do we try to batch any left over articles b4 we start? */
	master.grpnr = -1;	/* what group number are we currently on */
	master.skip_on_restart = FALSE;
	master.kill_log_name = N_KILLLOG;
	master.use_gui = FALSE;
	master.do_xover = TRUE;
	master.conn_dedupe = FALSE;
	master.post_filter= NULL;
	master.conn_active = FALSE;
	master.history_file = HISTORY_FILE;
	master.header_only = FALSE;
	master.db = -1;
	master.active_lastread = ACTIVE_DEFAULT_LASTREAD;
	master.use_xover = FALSE;
	master.xoverview = NULL;
	master.resetcounter = FALSE;
	master.low_read = FALSE;
	master.show_group = FALSE;
	
	/* have to do this next so if set on cmd line, overrides this */

#ifdef N_PHRASES		/* in case someone nukes def */
	if(stat(N_PHRASES, &sbuf) == 0 && S_ISREG(sbuf.st_mode)) {
		/* we have a regular phrases file make it the default */
		master.phrases = N_PHRASES;
	}
	
#endif
	
	/* allow no args, only the hostname, or hostname as first arg */
	/* also have to do the file argument checking */
	switch(argc) {
	  case 1:
		break;
	  case 2:
		/* the fargs == NULL test so only process first file name */
		if(argv[1][0] == FILE_CHAR) {
			if((fargs = build_args(&argv[1][1], &fargc)) != NULL) {
				retval = scan_args(&master, fargc, fargs);
			}
		}   
		else if(argv[1][0] == '-') {
			/* in case of suck -V */
			retval = scan_args(&master, 1, &(argv[1]));
		} 
		else{	
			master.host = argv[1];
		}
		break;
	  default:
		for(loop=1;loop<argc && fargs == NULL;loop++) {
			if(argv[loop][0] == FILE_CHAR) {
				if((fargs = build_args(&argv[loop][1], &fargc)) != NULL) {
					retval = scan_args(&master, fargc, fargs);
				}
			}   
		}
		/* this is here so anything at command line overrides file */
		if(argv[1][0] != '-' && argv[1][0] != FILE_CHAR) {
			master.host = 	argv[1];
			argc-= 2;
			args = &(argv[2]);
		}
		else {
			args = &(argv[1]);
			argc--;
		}
		retval = scan_args(&master, argc, args);	
		break;
	}
	/* print out status stuff */
	if(master.debug == TRUE) {
		do_debug("Suck version %s\n",SUCK_VERSION);
		do_debug("master.MultiFile = %d\n", master.MultiFile);
		do_debug("master.status_file = %d\n",master.status_file);
		do_debug("master.status_file_name = %s\n", null_str(master.status_file_name));
		do_debug("master.do_killfile = %s\n", true_str(master.do_killfile));
		do_debug("master.do_chkhistory = %s\n", true_str(master.do_chkhistory));
		do_debug("master.do_modereader = %s\n", true_str(master.do_modereader));
		do_debug("master.always_batch = %s\n", true_str(master.always_batch));
		do_debug("master.rnews_size = %ld\n", master.rnews_size);
		do_debug("master.batch = %d\n", master.batch);
		do_debug("master.batchfile = %s\n", null_str(master.batchfile));
		do_debug("master.cleanup = %s\n", true_str(master.cleanup));
		do_debug("master.host = %s\n", null_str(master.host));
		do_debug("master.portnr = %u\n", master.portnr);
		do_debug("master.pause_time = %d\n", master.pause_time);
		do_debug("master.pause_nrmsgs = %d\n", master.pause_nrmsgs);
		do_debug("master.sig_pause_time = %d\n", master.sig_pause_time);
		do_debug("master.sig_pause_nrmsgs = %d\n", master.sig_pause_nrmsgs);
		do_debug("master.killfile_log = %d\n", master.killfile_log);
		do_debug("master.phrases = %s\n", null_str(master.phrases));
		do_debug("master.errlog = %s\n", null_str(master.errlog));
		do_debug("master.rescan = %s\n", true_str(master.rescan == TRUE));
		do_debug("master.quiet = %s\n", true_str(master.quiet));
		do_debug("master.kill_ignore_postfix = %s\n", true_str(master.kill_ignore_postfix));
		do_debug("master.reconnect_nr=%d\n", master.reconnect_nr);
		do_debug("master.do_active = %s\n", true_str(master.do_active));
		do_debug("master.localhost = %s\n", null_str(master.localhost));
		do_debug("master.nrmode = %s\n", true_str(master.nrmode));
		do_debug("master.auto_auth = %s\n", true_str(master.auto_auth));
		do_debug("master.no_dedupe = %s\n", true_str(master.no_dedupe));
		do_debug("master.chk_msgid = %s\n", true_str(master.chk_msgid));
		do_debug("master.activefile = %s\n", null_str(master.activefile));
		do_debug("master.prebatch = %s\n", true_str(master.prebatch));
		do_debug("master.skip_on_restart = %s\n", true_str(master.skip_on_restart));
		do_debug("master.kill_log_name = %s\n", null_str(master.kill_log_name));
		do_debug("master.use_gui = %s\n", true_str(master.use_gui));
		do_debug("master.do_xover = %s\n", true_str(master.do_xover));
		do_debug("master.conn_dedupe = %s\n", true_str(master.conn_dedupe));
		do_debug("master.post_filter = %s\n", null_str(master.post_filter));
		do_debug("master.conn_active = %s\n", true_str(master.conn_active));
		do_debug("master.history_file = %s\n", null_str(master.history_file));
		do_debug("master.header_only = %s\n", true_str(master.header_only));
		do_debug("master.active_lastread = %d\n", master.active_lastread);
		do_debug("master.use_xover = %s\n", true_str(master.use_xover));
		
#ifdef TIMEOUT
		do_debug("TimeOut = %d\n", TimeOut);
#endif
		do_debug("master.debug = TRUE\n");
	}
        /* now do any final args checks needed */
	/* check to see if we have enough info to scan the localhost active file */
	if((master.do_active == TRUE || master.batch == BATCH_LIHAVE) && master.localhost == NULL) {
		retval = RETVAL_ERROR;
		error_log(ERRLOG_REPORT, suck_phrases[6], NULL);
	}
	
	/* okay now the main stuff */
	if(retval == RETVAL_OK) {
		if(master.status_file == FALSE) {
			/* if not in multifile mode, all status msgs MUST go to stderr to not mess up articles */
			/* this has to go before lockfile code, so lockfile prints msg to correct place. */
			master.msgs = ( master.MultiFile == FALSE) ? stderr : stdout ;
		}
#ifdef LOCKFILE
		/* this has to be here since we use full_path() to get path for lock file */
		/* and it isn't set up until we process the args above. */
		if(do_lockfile(&master) != RETVAL_OK) {
			exit(-1);
		}
#endif

#ifdef MYSIGNAL
		/* set up signal handlers */
#ifdef HAVE_SIGACTION
		sigemptyset(&(sigs.sa_mask));
		sigs.sa_handler = sighandler;
		sigs.sa_flags = 0;
		if(sigaction(MYSIGNAL, &sigs, NULL) == -1 || sigaction(PAUSESIGNAL, &sigs, NULL) == -1) {
			MyPerror(suck_phrases[67]);
		}
		else {
			signal_block(MYSIGNAL_SETUP);	/* set up sgetline() to block signal */ 
			pause_signal(PAUSE_SETUP, &master);	/* set up routine for pause swap if signal */
		}
#else
		/* the old-fashioned way */
		signal(MYSIGNAL, sighandler);
		signal(PAUSESIGNAL, sighander);
		pause_signal(PAUSE_SETUP, &master);
#endif	
#endif
		load_phrases(&master);	/* this has to be here so rest prints okay */

		/* set up status log, if none specified or unable to open status log */
		/* then  use stdout or stderr */

		if(master.status_file_name != NULL) {
			/* okay attempt to open it up */
			if((master.msgs = fopen(master.status_file_name, "a")) == NULL) {
				MyPerror(suck_phrases[0]);
				master.msgs = stdout;	/* reset to default */
			}
			else {
				master.status_file = TRUE;
			}
		}
#ifdef HAVE_SETVBUF
		setvbuf(master.msgs, NULL, _IOLBF, 0);	/* set to line buffering */
#endif
		/* do we batch up any lingering articles ? */
		if(master.prebatch == TRUE) {
			switch(master.batch) {
			case BATCH_FALSE:
				error_log(ERRLOG_REPORT, suck_phrases[58], NULL);
				break;
			case BATCH_INNXMIT:
				do_innbatch(&master);
				break;
			case BATCH_RNEWS:
				do_rnewsbatch(&master);
				break;
			case BATCH_LMOVE:
				do_lmovebatch(&master);
				break;
			case BATCH_LIHAVE:
				do_localpost(&master);
				break;
			}	   
		}
		
		/* now parse the killfile */
#ifdef KILLFILE
		if(master.do_killfile == TRUE) {
			master.killp = parse_killfile(KILL_KILLFILE, master.killfile_log, master.debug, master.kill_ignore_postfix);
		}
#endif
		/* now parse the xover killfile */
		if(master.do_xover == TRUE) {
			master.xoverp = parse_killfile(KILL_XOVER, master.killfile_log, master.debug, master.kill_ignore_postfix);
		}
		
		print_phrases(master.msgs, suck_phrases[1], master.host, NULL);		
		if(do_connect(&master, CONNECT_FIRST) != RETVAL_OK) {
			retval = RETVAL_ERROR;
		}
		else {	 
			/* if we have XOVER killfiles, we need to get the format of em before any processing */
			if(master.xoverp != NULL || master.use_xover == TRUE) {
				get_xoverview(&master);
				killprg_sendoverview(&master);
			}
			
			/* first, check for restart articles, */
			/* then scan for any new articles */
			if((loop = restart_yn(&master)) == RESTART_ERROR) {
				retval = RETVAL_ERROR;
			}
			else if(loop == RESTART_NO || master.rescan == TRUE) { 
				/* we don't do scan if we had restart and option = FALSE */
				/* do we scan the local active file? */
				if(master.do_active == TRUE || master.activefile != NULL) {
					if(get_message_index_active(&master) < RETVAL_OK) {
						retval = RETVAL_ERROR;
					}
				}
				else if(get_message_index(&master) < RETVAL_OK) {
					retval = RETVAL_ERROR;
				}
				if(retval == RETVAL_OK) {
					retval = do_supplemental(&master);
				}
				if(retval == RETVAL_OK) {
                                        retval = do_nodownload(&master);
                                }
				if(retval == RETVAL_OK && master.head != NULL && master.nritems > 0) {
					/* if we don't have any messages, we don't need to do this */
					if(master.no_dedupe == FALSE) {
						dedupe_list(&master);
					}
#ifdef CHECK_HISTORY
					if(master.do_chkhistory == TRUE) {
						chkhistory(&master);
					}
#endif
					print_phrases(master.msgs,suck_phrases[20], str_int(master.nritems), NULL);
				}
			}
			if(retval == RETVAL_OK) {
				if(master.nritems == 0) {
 					print_phrases(master.msgs,suck_phrases[3], NULL);
					retval = RETVAL_NOARTICLES;
				}
				else {
					/* write out restart db */
					retval = db_write(&master);
				/* reconnect after dedupe, in case of time out due to long dedupe time*/
					if(retval == RETVAL_OK && master.conn_dedupe == TRUE) {
						retval = do_connect(&master, CONNECT_AGAIN);
					}
					if(retval == RETVAL_OK) {
#ifdef USE_QUEUE
						retval = get_queue(&master);
#else
						retval = get_articles(&master);
#endif
					}
				}
				
			}
			/* send quit, and get reply */
			sputline(master.sockfd,"quit\r\n");
			if(master.debug == TRUE) {
				do_debug("Sending command: quit\n");
			}
			do {
				resp = sgetline(master.sockfd, &inbuf);
				if(resp>0) {
					if(master.debug == TRUE) {
						do_debug("Quitting GOT: %s", inbuf);
					}
					number(inbuf, &nr);					
				}
			
			}while(nr != 205 && resp > 0);	
		}
		if(master.sockfd >= 0) {
			close(master.sockfd);
			print_phrases(master.msgs,suck_phrases[4], master.host, NULL);
		}
		if(master.debug == TRUE) {
			do_debug("retval=%d (RETVAL_OK=%d), m.nrgot=%d, m.batch=%d\n", retval, RETVAL_OK, master.nrgot, master.batch);
		}
		if((retval == RETVAL_OK || master.always_batch == TRUE) && master.nrgot > 0 && master.header_only == FALSE) {
			switch(master.batch) {
			  case BATCH_INNXMIT:
				  do_post_filter(&master);
				  do_innbatch(&master);
				  break;
			  case BATCH_RNEWS:
				  do_post_filter(&master);
				  do_rnewsbatch(&master);
				  break;
			  case BATCH_LMOVE:
				  do_post_filter(&master);
				  do_lmovebatch(&master);
				  break;
			  case BATCH_LIHAVE:
				  do_post_filter(&master);
				  do_localpost(&master);
				  break;  
			  default:
				  break;
			}
		}
		if((retval == RETVAL_NOARTICLES || retval == RETVAL_OK) && master.cleanup == TRUE) {
			print_phrases(master.msgs, suck_phrases[7], NULL);
			do_cleanup(&master);
		}
		
		/* close out status log */
		if(master.msgs != NULL && master.msgs != stdout && master.msgs != stderr) {
			fclose(master.msgs);
		}
		if(master.head != NULL) {
			/* clean up memory */
			master.curr = master.head;
			while(master.curr != NULL) {
				temp = (master.curr)->next;
				free_one_node(master.curr);
				master.curr = temp;
			}
		}
		/* free up group list */
		while (master.groups != NULL) {
			ptemp=(master.groups)->next;
			free(master.groups);
			master.groups = ptemp;
		}
		/* free up overview.fmt list */
		while (master.xoverview != NULL) {
			pov = (master.xoverview)->next;
			if((master.xoverview)->header != NULL) {
				free((master.xoverview)->header);
			}
			free(master.xoverview);
			master.xoverview = pov;
		}
		
#ifdef KILLFILE
		free_killfile(master.killp);
#endif
		free_killfile(master.xoverp);
		
		if(fargs != NULL) {
			free_args(fargc, fargs);
		}		
#ifdef LOCKFILE
		lockfile = full_path(FP_GET, FP_TMPDIR, N_LOCKFILE);	
		if(lockfile != NULL) {
			unlink(lockfile);
		}
#endif	
	}
	free_phrases();	/* do this last so everything is displayed correctly */
	exit(retval);
}
/*------------------------------------------------------------*/
int do_connect(PMaster master, int which_time) {

	char *inbuf;
	int nr, resp, retval = RETVAL_OK;
	struct hostent *hi;
	FILE *fp;
	
	
	if(which_time != CONNECT_FIRST) {
		/* close down previous connection */
		sputline(master->sockfd, "quit\r\n");
		do {
			resp = sgetline(master->sockfd, &inbuf);
			if(resp>0) {
				if(master->debug == TRUE) {
					do_debug("Reconnect GOT: %s", inbuf);
				}
				number(inbuf, &nr);
			}
			
		}while(nr != 205 && resp > 0);		
		close(master->sockfd);

		/* now reset everything */
		if(master->curr != NULL) {
			(master->curr)->sentcmd = FALSE;
		}
		master->grpnr = -1;		
	}
	
	if(master->debug == TRUE) {
		do_debug("Connecting to %s on port %d\n", master->host, master->portnr);
	}
	fp = (which_time == CONNECT_FIRST) ? master->msgs : NULL;
	master->sockfd = connect_to_nntphost( master->host, &hi, fp , master->portnr);
	if(master->sockfd < 0 ) {
		retval = RETVAL_ERROR;
	}
	else if(sgetline(master->sockfd, &inbuf) < 0) {
	 	/* Get the announcement line */
		retval = RETVAL_ERROR;
	}
	else {
		if(master->debug == TRUE) {
			do_debug("Got: %s", inbuf);
		}
		if(which_time == CONNECT_FIRST) {
			fprintf(master->msgs ,"%s", inbuf );
		}
		/* check to see if we have to do authorization now */
		number(inbuf, &resp);
		if(resp == 480 ) {
			retval = do_authenticate(master);
		}		
		if(retval == RETVAL_OK && master->do_modereader == TRUE) {
			retval = send_command(master, "mode reader\r\n", &inbuf, 0);
			if(retval == RETVAL_OK) {			
				/* Again the announcement */
				if(which_time == CONNECT_FIRST) {
					fprintf(master->msgs ,"%s",inbuf);
				}
			}
		}
		if(master->auto_auth == TRUE) {
			if(master->passwd == NULL || master->userid == NULL) {
				error_log(ERRLOG_REPORT, suck_phrases[55], NULL);
				retval = RETVAL_ERROR;
			}
			else {
				
				/* auto authorize */
				retval = do_authenticate(master);
			}
		}
	}
	return retval;
}

/*--------------------------------------------------------------------*/
int get_message_index(PMaster master) {

	long lastread;
	int nrread, retval, maxread;
	
	char buf[MAXLINLEN], group[512];
	FILE *ifp,*tmpfp;

	retval = RETVAL_OK;
	ifp = tmpfp = NULL;

	TimerFunc(TIMER_START, 0, NULL);

	if((ifp = fopen(full_path(FP_GET, FP_DATADIR, N_OLDRC), "r" )) == NULL) {
		MyPerror(full_path(FP_GET, FP_DATADIR, N_OLDRC));
		retval = RETVAL_ERROR;
	}
	else if((tmpfp = fopen(full_path(FP_GET, FP_TMPDIR, N_NEWRC), "w" )) == NULL) {
		MyPerror(full_path(FP_GET, FP_TMPDIR, N_NEWRC));
		retval = RETVAL_ERROR;
	}
	while(retval == RETVAL_OK && fgets(buf, MAXLINLEN-1, ifp) != NULL) {
		if(buf[0] == SUCKNEWSRC_COMMENT_CHAR) {
			/* skip this group */
			fputs(buf, tmpfp);
			print_phrases(master->msgs, suck_phrases[8], buf, NULL);
		}
		else {
			maxread = -1; /* just in case */
			nrread = sscanf(buf, "%s %ld %d\n", group, &lastread, &maxread);
			if ( nrread < 2 || nrread > 3) {
				error_log(ERRLOG_REPORT, suck_phrases[9], buf, NULL);
				fputs(buf, tmpfp);	/* rewrite the line */
			}
			else if(maxread == 0) {
				/* just rewrite the line */
				fputs(buf, tmpfp);
			}
			else {
				retval = do_one_group(master, buf, group, tmpfp, lastread, maxread);
			}
		}
	}

	TimerFunc(TIMER_TIMEONLY, 0,master->msgs);

	if(retval == RETVAL_OK) {
		print_phrases(master->msgs, suck_phrases[16], str_int(master->nritems), NULL);
	}
	else if(ifp != NULL) {
		/* this is in case we had to abort the above while loop (due to loss of pipe to server) and */
		/* we hadn't finished writing out the suck.newrc, this finishes it up. */
		do {
			fputs(buf, tmpfp);
		}
		while(fgets(buf, MAXLINLEN-1, ifp) != NULL);
	}
	if(tmpfp != NULL) {
		fclose(tmpfp);
	}
	if(ifp != NULL) {
		fclose(ifp);
	}
	return retval;
}
/*-----------------------------------------------------------------------------------------------------*/
int do_one_group(PMaster master, char *buf, char *group, FILE *newrc, long lastread, int maxread) {

	char *sp, *inbuf, cmd[MAXLINLEN];
	long count,low,high;
	int response,retval,i;
	
	retval = RETVAL_OK;
	

	sprintf(cmd,"group %s\r\n",group);
	if(send_command(master,cmd,&inbuf,0) != RETVAL_OK) {
		retval = RETVAL_ERROR;
	}
	else {
		sp = number(inbuf, &response);
		if(response != 211) {
			fputs(buf, newrc);	/* rewrite line AS IS in newrc */
				/* handle group not found */
			switch(response) {
			case 411:
				error_log(ERRLOG_REPORT, suck_phrases[11], group, NULL);
				break;
			case 500:
				error_log(ERRLOG_REPORT, suck_phrases[48], NULL);
				break;
			default:
				error_log(ERRLOG_REPORT, suck_phrases[12],group,str_int(response),NULL);
				retval = RETVAL_ERROR; /* bomb out on wacko errors */
				break;
			}
		}
		else {
			sp = get_long(sp, &count);
  			sp = get_long(sp, &low);
			sp = get_long(sp, &high);
  			fprintf(newrc, "%s %ld", group, high);
			
			if(maxread > 0) {
				fprintf(newrc, " %d", maxread);
			}
			fputs("\n", newrc);
  			
  			/* add a sanity check in case remote host changes its numbering scheme */
			/* the > 0 is needed, since if a nnrp site has no article it will reset */
			/* the count to 0.  So not an error */
  			if(lastread > high && high > 0) {
				if(master->resetcounter == TRUE) {
					/* reset lastread to low, so we pick up all articles in the group */
					lastread = low;
					print_phrases(master->msgs,suck_phrases[71],group,str_int(high),str_int(low),NULL);
				}
				else {
					print_phrases(master->msgs,suck_phrases[13],group,str_int(high),NULL);
				}
  			}

			if(lastread < 0 ) {
				/* if a neg number, get the last X nr of messages, handy for starting */
				/* a new group off ,say -100 will get the last 100 messages */
				lastread += high;	/* this works since lastread is neg */
				if(lastread < 0) {
					lastread = 0;	/* just to be on the safeside */
				}
			}

			/* this has to be >= 0 since if there are no article on server high = 0 */
			/* so if we write out 0, we must be able to handle zero as valid lastread */
			/* the count > 0 so if no messages available we don't even try */
			/* or if low > high no messages either */
			if (low <= high && count > 0 && lastread < high && lastread >= 0) {
				/* add group name to list of groups */
				
   				if(lastread < low) {
					lastread = low - 1;
				}
				/* now set the max nr of messages to read */
				if(maxread > 0 && high-maxread > lastread) {
					if(master->low_read == TRUE) {
						/* instead of limiting from the high-water mark (the latest articles) */
						/* limit from the low-water mark (the oldest articles) */
						high = (lastread+1) + maxread;
					}
					else {
						lastread = high-maxread;
					}
					print_phrases(master->msgs, suck_phrases[14], group, str_int(maxread), NULL);
				}
				print_phrases(master->msgs, suck_phrases[15], group, str_long(high-lastread), str_long(lastread+1), str_long(high), NULL);
				if(master->xoverp != NULL) {
					/* do this via the xover killfile command */
					retval = do_group_xover(master, group, lastread+1, high);
				}
				/* do we use xover or xhdr to get our article list */
				if(master->use_xover == TRUE) {
					retval = get_xover(master, group, lastread+1, high);
				}
				else if((master->xoverp == NULL) || (retval == RETVAL_NOXOVER)) {
					/* do this the normal way */
					sprintf(cmd, "xhdr Message-ID %ld-%ld\r\n", lastread+1,high);
					i = send_command(master,cmd,&inbuf,221);
					if(i != RETVAL_OK) {
						retval = RETVAL_ERROR;
					}
					else {
						do {
							if(sgetline(master->sockfd, &inbuf) < 0) {
								retval = RETVAL_ERROR;
							}
							else if (*inbuf != '.' ) {
								retval = allocnode(master, inbuf, MANDATORY_OPTIONAL, group, 0L);
							}
						} while (retval == RETVAL_OK && *inbuf != '.' && *(inbuf+1) != '\n');
					} /* end if response */
				} /* end if xover */	
  			} /* end if lastread */
		} /* end response */
	} /* end else */

	if(retval == RETVAL_ERROR) {
		error_log(ERRLOG_REPORT, suck_phrases[59], NULL);
	}
	return retval;
}
/*-----------------------------------------------------------*/
int get_articles(PMaster master) {

	int retval, logcount, i, grpnr, grplen;
	long loop, downloaded;
	PGroups grps;
	const char *grpname;
	const char *empty = "";
	
#ifdef HAVE_GETTIMEOFDAY
	double bps;
#endif
	
#ifdef KILLFILE
	int ( *get_message)(PMaster, int, long); /* which function will we use get_one_article or get_one_article_kill) */

	grpnr = -1;
	grps = NULL;
	grpname = empty;
	grplen = 0;
		/* do we use killfiles? */
	get_message = (master->killp == NULL) ? get_one_article : get_one_article_kill;
#endif
	retval = RETVAL_OK;
	downloaded = loop = 0; /* used to track how many downloaded, for reconnect option */
	
	/* figure out how many digits wide the articleCount is for display purposes */
	/* this used to be log10()+1, but that meant I needed the math library */ 
	for(logcount=1, i=master->nritems; i > 9 ; logcount++) {
		i /= 10;
	}
 
	if(master->MultiFile == TRUE && checkdir(full_path(FP_GET, FP_MSGDIR, NULL)) == FALSE) {
		retval = RETVAL_ERROR;
	}
	else {
		retval = db_open(master);
		if(retval == RETVAL_OK) {
			master->curr = master->head;

			if(master->batch == BATCH_INNFEED) {
				/* open up batch file for appending to */
				if((master->innfeed = fopen(master->batchfile, "a")) == NULL) {
					MyPerror(master->batchfile);
				}
			}
			else if(master->batch == BATCH_LIHAVE) {
				if((master->innfeed = fopen(full_path(FP_GET, FP_TMPDIR, master->batchfile), "a")) == NULL) {
					MyPerror(full_path(FP_GET, FP_TMPDIR, master->batchfile));
					retval = RETVAL_ERROR;
				}
			}

			TimerFunc(TIMER_START, 0, NULL);

#ifdef MYSIGNAL
			while(retval == RETVAL_OK && master->curr != NULL && GotSignal == FALSE) {
#else
			while(retval == RETVAL_OK && master->curr != NULL) {
#endif
				if(master->debug == TRUE) {
					do_debug("Article nr = %s mandatory = %c\n", (master->curr)->msgnr, (master->curr)->mandatory);
				}
				loop++;
				if((master->curr)->downloaded == FALSE) {
					/* we haven't yet downloaded this article */
					downloaded++;
					
				        /* to be polite to the server, lets allow for a pause every once in a while */
					if(master->pause_time > 0 && master->pause_nrmsgs > 0) {
						if((downloaded > 0) && (downloaded % master->pause_nrmsgs == 0)) {
							sleep(master->pause_time);
						}
					}
					if(master->status_file == FALSE && master->quiet == FALSE) {
						/* if we are going to a file, we don't want all of these articles printed */
						/* or if quiet flag is set */
						/* this stuff doesn't go thru print_phrases so I can keep spacing right */
						/* and I only print numbers and the BPS */
#ifdef HAVE_GETTIMEOFDAY
						bps = TimerFunc(TIMER_GET_BPS, 0, master->msgs);
#endif
						if(master->use_gui == TRUE) {
							/* this stuff is formatted by the GUI so we put it out in a format it likes */
#ifndef HAVE_GETTIMEOFDAY
							fprintf(master->msgs, "---%ld+++\n",master->nritems - loop);
#else
							fprintf(master->msgs, "---%ld+++%f\n", master->nritems - loop, bps);
#endif /* HAVE_GETTIMEOFDAY */
						}
						else {
							if(master->show_group == TRUE) {
							/* add the group name (if available) to the line printed */
								if((master->curr)->groupnr >= 0) {
									if((master->curr)->groupnr != grpnr) {
										grpname = empty;
										grpnr = (master->curr)->groupnr;
										/* new group name find it */
										grps = master->groups;
										while(grps != NULL && grps->nr != grpnr) {
											grps = grps->next;
										}
										if(grps != NULL) {
											grpname = grps->group;
											/* calculate max len for format of printf*/
											/* which erases the previous group name */
											if(strlen(grpname) > grplen) {
												grplen = strlen(grpname);
											}
										}
									}		
								}
								else {
									grpnr = -1;
									grpname = empty;
								}
							}
#ifndef HAVE_GETTIMEOFDAY
							fprintf(master->msgs, "%5ld %-*s\r",master->nritems  - loop, grplen, grpname);
#else
							fprintf(master->msgs, "%5ld %9.1f %s %-*s\r",master->nritems-loop,bps,suck_phrases[5], grplen, grpname);
#endif /* HAVE_GETTIMEOFDAY */
						}
						fflush(master->msgs);	/* so message gets printed now */
					}
#ifdef KILLFILE
				        /* get one article */
					retval = (*get_message)(master, logcount, loop);
#else
					retval = get_one_article(master, logcount, loop);
#endif
					if(retval == RETVAL_OK ) {
						retval = db_mark_dled(master, master->curr);
					}
				}
				
				master->curr = (master->curr)->next; 	/* get next article */

				/* to be NOT polite to the server reconnect every X msgs to combat the INND */
				/* LIKE_PULLERS=DONT.  This is last in loop, so if problem can abort gracefully */
				if((master->reconnect_nr > 0) && (downloaded > 0) && (downloaded % master->reconnect_nr == 0)) {
					retval =  do_connect(master, CONNECT_AGAIN);
					if(master->curr != NULL) {
						(master->curr)->sentcmd = FALSE;	/* if we sent command force resend */
					}
				} 

 
		 	} /* end while */
			db_close(master);
			if(retval == RETVAL_OK && master->nritems == loop) {
				db_delete(master);
			}

			if(retval == RETVAL_OK) {
				TimerFunc(TIMER_TOTALS, 0, master->msgs);
			}
			if(master->innfeed != NULL) {
				fclose(master->innfeed);
			}
		}
	}
	return retval;
}
/*-----------------------------------------------*/
/* add items from supplemental list to link list */
/* ----------------------------------------------*/ 
int do_supplemental(PMaster master) {

	int retval, oldkept;
	FILE *fp;
	char linein[MAXLINLEN+1];

	retval = RETVAL_OK;
	oldkept = master->nritems;		

	if((fp = fopen(full_path(FP_GET, FP_DATADIR, N_SUPPLEMENTAL), "r")) != NULL) {
		print_phrases(master->msgs, suck_phrases[17], NULL);
		while(retval == RETVAL_OK && fgets(linein, MAXLINLEN, fp) != NULL) {
			if(linein[0] == '!') {
				retval = do_sup_bynr(master, linein);
			}
			else if(linein[0] == '<') {
				retval = allocnode(master, linein, MANDATORY_YES, NULL, 0L);
			}
			else {
				error_log(ERRLOG_REPORT, suck_phrases[18], linein, NULL);
			}
		}
		print_phrases(master->msgs, suck_phrases[19], str_int(master->nritems-oldkept), \
			str_int(master->nritems), NULL);
		fclose(fp);
	}
	
	return retval;
}
/*------------------------------------------------------------------------------------------*/
 int do_sup_bynr(PMaster master, char *linein) {
	 int retval = RETVAL_OK;
	 /* this routine takes in a line formated !group_name article_nr */
	 /* gets it's Msg-ID (via XHDR), then adds it to our list of articles */
	 /* as a mandatory article to download */

	 char grpname[MAX_GRP_LEN], cmd[MAXLINLEN], *resp;
	 int i, done;
	 long nrlow, nrhigh;
	 
	 if(master->debug == TRUE) {
		 do_debug("supplemental adding %s", linein);
	 }
	 i = sscanf(linein, "!%s %ld-%ld", grpname, &nrlow, &nrhigh);
	 if(i < 2) {
		 error_log(ERRLOG_REPORT, suck_phrases[18], linein, NULL);
	 }
	 else {
		 sprintf(cmd,"group %s\r\n",grpname);
		 if(send_command(master,cmd,NULL,211) == RETVAL_OK) {
			 if(i == 2) {
				 sprintf(cmd,"xhdr Message-ID %ld\r\n",nrlow);
			 }
			 else {
				 sprintf(cmd,"xhdr Message-ID %ld-%ld\r\n",nrlow,nrhigh);
			 }
			 if(send_command(master,cmd,NULL,221) == RETVAL_OK) {
				 /* now have to get message-id and the . */
				 done = FALSE;
				 while( done == FALSE) {
					 if(sgetline(master->sockfd, &resp) < 0) {
						 retval = RETVAL_ERROR;
						 done = TRUE;
					 }
					 else {
						 if(master->debug == TRUE) {
							 do_debug("Got %s", resp);
						 }
						 if(*resp == '.') {
							 done = TRUE;
						 }
						 else {
							 retval = allocnode(master, resp, MANDATORY_YES, grpname, 0L);
						 }
					 }
				 }
			 }
		 }	 
	 }
	 
	 return retval;
	 
 }
 /*---------------------------------------------------------------------*/
 int do_nodownload(PMaster master) {
	 int i, retval = RETVAL_OK;
	 FILE *fp;
	 PList item, prev;
	 char linein[MAXLINLEN+1];
	 long nrlines = 0, nrnuked = 0;
	 
	 if((fp = fopen(full_path(FP_GET, FP_DATADIR, N_NODOWNLOAD), "r")) != NULL) {
		print_phrases(master->msgs, suck_phrases[68], NULL);
		while(retval == RETVAL_OK && fgets(linein, MAXLINLEN, fp) != NULL) {
			nrlines++;
			i = strlen(linein) - 1; /* so it points at last char */
			/* strip off nl */
			if(linein[i] == '\n') {
				linein[i] = '\0';
				i--;
			}
			/* strip off extra spaces on the end */
			while(isspace(linein[i])){
				linein[i] = '\0';
				i--;
			}
			item = master->head;
			prev = NULL;
			/* find start of msgid */
			if(linein[0] != '<') {
				error_log(ERRLOG_REPORT, suck_phrases[69], linein, NULL);
			}
			else {
				if(master->debug == TRUE) {
					do_debug("Checking Nodownload - %s\n", linein);
				}
				
				while((item != NULL) && (cmp_msgid(linein, item->msgnr) == FALSE)) {
					prev = item;
					item = item->next;
				}
				if(item != NULL) {
					/* found a match, remove from the list */
					if(master->debug == TRUE) {
						do_debug("Matched, nuking from list\n");
					}
					nrnuked++;
					master->nritems--;
					if(item == master->head) {
						/* change top of list */
						master->head = (master->head)->next;
					}
					else {
                                                 /* its not the end of the list */
						prev->next = item->next;
						free_one_node(item);
					}
				}
			}
		}
		fclose(fp);
		print_phrases(master->msgs, suck_phrases[70], str_long(nrlines), str_long(nrnuked), str_long(master->nritems), NULL);
	}
	return retval;
 }
/*----------------------------------------------------------------------*/
 int get_group_number(PMaster master, char *grpname) {

	 PGroups grps, gptr;
	 int groupnr = 0;
	 
         /* first, find out if it doesn't exist already */
	 gptr = master->groups;
	 while(gptr != NULL && groupnr == 0) {
		 if(strcmp(gptr->group, grpname) == 0) {
				/* bingo */
			 groupnr = gptr->nr;
		 }
		 else {
			 gptr = gptr->next;
		 }
	 }
	 
	 if(groupnr == 0) {
		 /* add group to group list and get group nr */
		 if((grps = malloc(sizeof(Groups))) == NULL) {
				/* out of memory */
			 error_log(ERRLOG_REPORT, suck_phrases[22], NULL);
		 }
		 else {
			 grps->next = NULL;
			 strcpy(grps->group, grpname);
				/* now add to list and count groupnr */
			 if(master->groups == NULL) {
				 grps->nr = 1;
				 master->groups = grps;
			 }
			 else {
				 gptr = master->groups;
				 while(gptr->next != NULL) {
					 gptr = gptr->next;
				 }
				 gptr->next = grps;
				 grps->nr = gptr->nr + 1;
			 }
			 groupnr = grps->nr;
				 
			 if(master->debug == TRUE) {
				 do_debug("Adding to group list: %d %s\n", grps->nr, grps->group);
			 }
		 }
	 }
	 return groupnr;
	 
 }		 
/*-----------------------------------------------------------------------*/
int allocnode(PMaster master, char *linein, int mandatory, char *group, long msgnr_in) {
	/* if msgnr_in is not filled in (0), then parse the msgnr off the linein */
	
	/* if allocate memory here, must free in free_one_node */
	
	PList ptr = NULL;
	char *end_ptr, *st_ptr;
	static PList curr = NULL;	/* keep track of current end of list */
	int groupnr = 0, retval = RETVAL_OK;
	long msgnr = 0;
	
	static int warned = FALSE;

	/* get the article nr */
	if(group == NULL) {
		/* we're called by do_supplemental */
		st_ptr = linein;
	}
	else {
		if (msgnr_in > 0) {
			/* we're prob called by xover code */
			st_ptr = linein;
			msgnr = msgnr_in;
		}
		else {
			st_ptr = get_long(linein,&msgnr);
			if(msgnr == 0 && warned == FALSE) {
				warned = TRUE;
				error_log(ERRLOG_REPORT, suck_phrases[53], NULL);
			}
		}
		
		if(msgnr > 0 && group != NULL) {
			groupnr = get_group_number(master, group);
		}
	}
	
	if(retval == RETVAL_OK) {
		/* find the message id */
		while(*st_ptr != '<' && *st_ptr != '\0') {
			st_ptr++;
		}
		end_ptr = st_ptr;
		while(*end_ptr != '>' && *end_ptr != '\0') {
			end_ptr++;
		}
		if((*st_ptr != '<') || ( master->chk_msgid == TRUE && *end_ptr != '>')) {
			error_log(ERRLOG_REPORT, suck_phrases[21], linein, NULL);
		}
		else {
			*(end_ptr+1) = '\0';	/* ensure null termination */
		
			if((ptr = malloc(sizeof(List))) == NULL) {
				error_log(ERRLOG_REPORT, suck_phrases[22], NULL);
				retval = RETVAL_ERROR;
			}
			else if(strlen(st_ptr) >= MAX_MSGID_LEN) {
				error_log(ERRLOG_REPORT, suck_phrases[63], NULL);
				free(ptr);
			}
			else {
				strcpy(ptr->msgnr, st_ptr);
				ptr->next = NULL;
				ptr->mandatory = (char) mandatory;
				ptr->sentcmd = FALSE;		/* have we sent command to remote */
				ptr->nr = msgnr;
				ptr->groupnr = groupnr;
				ptr->downloaded = FALSE;
				ptr->dbnr = 0L;
				ptr->delete = FALSE;
				
				if(master->debug == TRUE) {
					do_debug("MSGID %s NR %d GRP %d MANDATORY %c added\n",ptr->msgnr, ptr->nr, ptr->groupnr, ptr->mandatory);
				}
			
				/* now put on list */
				if( curr == NULL) {
					if(master->head == NULL) {
				                /* first node */
						master->head = curr = ptr;
					}
					else {
						/* came in with a restart, find end of list */
						curr = master->head;
						while(curr->next != NULL) {
							curr = curr->next;
						}
						
						/* now add node on */
						curr->next = ptr;
						curr = ptr;
					}	
				}
				else {
					curr->next = ptr;
					curr = ptr;
				}
				master->nritems++;
			}
		}
	}
	
	return retval;
}
/*------------------------------------------------------------------------*/
 void free_one_node(PList node) {
		 
	free(node);
}
/*----------------------------------------------------------------------------------*/
 const char *build_command(PMaster master, const char *cmdstart, PList article) {
	 static char cmd[MAXLINLEN+1];
	 static int warned = FALSE;
	 char *resp;
	 char grpcmd[MAXLINLEN];
	 
	 PGroups grps;
	 
	 /* build command to get article/head/body */
	 /* if nrmode is on send group cmd if needed */
	 if(master->nrmode == TRUE) {
		 if(article->nr == 0) {
			 master->nrmode = FALSE;
		 }
		 else if(master->grpnr != article->groupnr) {
			 grps = master->groups;
			 while(grps != NULL && grps->nr != article->groupnr) {
				 grps = grps->next;
			 }
			 if(grps != NULL) {
				 /* okay send group command */
				 sprintf(grpcmd, "GROUP %s\r\n", grps->group);
				 if(send_command(master, grpcmd, &resp, 211) != RETVAL_OK) {
					 master->nrmode = FALSE; /* can't chg groups turn it off */
				 }
				 else {
					 /* so don't redo group command on next one */
					 master->grpnr = grps->nr;
				 }
			 }	    
		 }
		 if(master->nrmode == TRUE) {
			 /* everything hunky dory, we've switched groups, and got a good nr */
			 sprintf(cmd, "%s %ld\r\n", cmdstart, article->nr);
		 }
		 else if(warned == FALSE) {
			 /* tell of the switch to nonnr mode */
			 warned = TRUE;
			 error_log(ERRLOG_REPORT, suck_phrases[54], NULL);
		 }
	 }
	 /* the if is in case we changed it above */
	 if(master->nrmode == FALSE) {
		 sprintf(cmd, "%s %s\r\n", cmdstart, article->msgnr);
	 }
	 return cmd;
 }
/*----------------------------------------------------------------------------------*/
int get_one_article(PMaster master, int logcount, long itemon) {

	int nr, len, retval = RETVAL_OK;
	char buf[MAXLINLEN+1];
	const char *cmd = "", *tname = NULL;
	char fname[PATH_MAX+1];

	char *resp;
	PList plist;
	
	FILE *fptr = stdout;	/* the default */

	fname[0] = '\0';	/* just in case */
	
	/* first send command to get article if not already sent */
	if((master->curr)->sentcmd == FALSE) {
		cmd = (master->header_only == FALSE)
			? build_command(master, "article", master->curr)
			: build_command(master, "head", master->curr); 
		if(master->debug == TRUE) {
			do_debug("Sending command: \"%s\"",cmd);
		}
		sputline(master->sockfd, cmd);
		(master->curr)->sentcmd = TRUE;
	}
	plist = (master->curr)->next;
	if(plist != NULL) {
		if(master->nrmode == FALSE || plist->groupnr == (master->curr)->groupnr) {
		/* can't use pipelining if nrmode on, cause of GROUP command I'll have to send*/
			if(plist->sentcmd == FALSE) {
				/* send next command */
				cmd = (master->header_only == FALSE)
					? build_command(master, "article", plist)
					: build_command(master, "head", plist); 
				if(master->debug == TRUE) {
					do_debug("Sending command: \"%s\"",cmd);
				}
				sputline(master->sockfd, cmd);
				plist->sentcmd = TRUE;
			}
		}
	}

	/* now while the remote is finding the article lets set up */

	if(master->MultiFile == TRUE) {
		/* open file */
		/* file name will be ####-#### ex 001-166 (nron,total) */
		sprintf(buf,"%0*ld-%d", logcount, itemon ,master->nritems);
		/* the strcpy to avoid wiping out fname in second call to full_path */
		strcpy(fname, full_path(FP_GET, FP_MSGDIR, buf));
		strcat(buf, N_TMP_EXTENSION);		/* add tmp extension */
		tname = full_path(FP_GET, FP_TMPDIR, buf);	/* temp file name */

		if(master->debug == TRUE) {
			do_debug("File name = \"%s\" temp = \"%s\"", fname, tname);
		}
		if((fptr = fopen(tname, "w")) == NULL) {
			MyPerror(tname);
			retval = RETVAL_ERROR;
		}
	}

	/* okay hopefully by this time the remote end is ready */
	if((len = sgetline(master->sockfd, &resp)) < 0) {
		retval = RETVAL_ERROR;
	}
	else {
		if(master->debug == TRUE) {
			do_debug("got answer: %s", resp);
		}

		TimerFunc(TIMER_ADDBYTES, len, NULL);

		number(resp, &nr);
		if(nr == 480) {
			/* got to authorize, negate send-ahead for next msg */
			plist->sentcmd = FALSE;	/* cause its gonna error out in do_auth() */
			if(do_authenticate(master) == RETVAL_OK) {
				/* resend command for current article */
				cmd = build_command(master, "article", master->curr);
				if(master->debug == TRUE) {
					do_debug("Sending command: \"%s\"",cmd);
				}
				sputline(master->sockfd, cmd);
				len = sgetline(master->sockfd, &resp);
				if(master->debug == TRUE) {
					do_debug("got answer: %s", resp);
				}
				TimerFunc(TIMER_ADDBYTES, len, NULL);
				number(resp, &nr);		
			}
			else {
				retval = RETVAL_ERROR;
			}
		}	
		/* 221 = header 220 = article */
		if(nr == 220 || nr == 221 ) {
			/* get the article */
			if((retval = get_a_chunk(master->sockfd, fptr)) == RETVAL_OK) {
				master->nrgot++;
				if (fptr == stdout) {
					/* gracefully end the message in stdout version */
					fputs(".\n", stdout);
				}
			}
		}
		else
			{
			/* don't return RETVAL_ERROR so we can get next article */
			error_log(ERRLOG_REPORT, suck_phrases[29], cmd ,resp, NULL);
		}
	}
	if(fptr != NULL && fptr != stdout) {
		fclose(fptr);
	}
	
	if(master->MultiFile == TRUE) {
		if(retval == RETVAL_ERROR || (nr != 221 && nr != 220)) {
			unlink(tname);
		}
		/* now rename it to the permanent file name */
		else {
			move_file(tname, fname);
			if((master->batch == BATCH_INNFEED || master->batch == BATCH_LIHAVE) && master->innfeed != NULL) {
				/* write path name and msgid to file */
				fprintf(master->innfeed, "%s %s\n", fname, (master->curr)->msgnr);
				fflush(master->innfeed); /* so it gets written sooner */
			}
		}
	}
	return retval;
}
/*---------------------------------------------------------------------------*/
int get_a_chunk(int sockfd, FILE *fptr) {

	int done, partial, len, retval;
	char *inbuf;
	size_t nr;
	
	retval = RETVAL_OK;
	done = FALSE;
	partial = FALSE;
	len = 0; /* just to get rid of a compiler warning */
	/* partial = was the previous line a complete line or not */
	/* this is needed to avoid a scenario where the line is MAXLINLEN+1 */
	/* long and the last character is a ., which would make us think */
	/* that we are at the end of the article when we actually aren't */
	
	while(done == FALSE && (len = sgetline(sockfd, &inbuf)) >= 0) {

		TimerFunc(TIMER_ADDBYTES, len, NULL);
			
		if(inbuf[0] == '.' && partial == FALSE) {
			if(len == 2 && inbuf[1] == '\n') {
				done = TRUE;
			}
			else if(fptr != stdout) {
				/* handle double dots IAW RFC977 2.4.1*/
				/* if we are going to stdout, we have to leave double dots alone */
				/* since it might mess up the .\n for EOM */
				inbuf++;	/* move past first dot */
				len--;
			}
		}
		if(done == FALSE) {
			if(retval == RETVAL_OK) {
				nr = fwrite(inbuf, sizeof(inbuf[0]), len, fptr);
				fflush(fptr);
				if(nr != len) {
					retval = RETVAL_ERROR;
					error_log(ERRLOG_REPORT, suck_phrases[57], NULL);
				}
			}
			partial= (len==MAXLINLEN&&inbuf[len-1]!='\n') ? TRUE : FALSE;
 		} 
	}
	if(len < 0) {
		retval = RETVAL_ERROR;
	}
	return retval;
}
/*-----------------------------------------------------------------------------*/
int restart_yn(PMaster master) {
	int done, retval = RESTART_NO;
	PList itemon;
	long itemnr;
	const char *fname;
	struct stat buf;
	
	fname = full_path(FP_GET, FP_TMPDIR, N_DBFILE);
	if(stat(fname, &buf) == 0) {
		/* restart file exists */
		retval = db_read(master);
		if(retval == RETVAL_OK) {
			/* find out where we stopped */
			itemon = master->head;
			itemnr = 0L;
			done = FALSE;
			while(itemon != NULL && done == FALSE) {
				itemnr++;
				if(itemon->downloaded == FALSE) {
					/* so that we don't think we already sent command */
					itemon->sentcmd = FALSE;
					if(master->skip_on_restart == TRUE) {
						/* mark one more */
						print_phrases(master->msgs, suck_phrases[60], NULL);
						itemon->downloaded = TRUE;
					}
					else {
						itemnr--; /* don't count this one */
					}
					done = TRUE;
				}
				itemon = itemon->next;
			}
		}
	}
	return retval;
}
/*-----------------------------------------------------------------*/
#ifdef MYSIGNAL
RETSIGTYPE sighandler(int what) {

	switch(what) {
	  case PAUSESIGNAL:
		pause_signal(PAUSE_DO, NULL);
		/* if we don't do this, the next time called, we'll abort */
/*		signal(PAUSESIGNAL, sighandler); */
/*		signal_block(MYSIGNAL_SETUP); */	/* just to be on the safe side */ 
		break;
	  case MYSIGNAL:
	  default:
		error_log(ERRLOG_REPORT, suck_phrases[24], NULL);
		GotSignal = TRUE;
	}
}
/*----------------------------------------------------------------*/
void pause_signal(int action, PMaster master) {

	int x, y;
	static PMaster psave = NULL;

	switch(action) {
	  case PAUSE_SETUP:
		psave = master;
		break;
	  case PAUSE_DO:
		if(psave == NULL) {
			error_log(ERRLOG_REPORT, suck_phrases[25], NULL);
		}
		else {
			/* swap pause_time and pause_nrmsgs with the sig versions */
			x = psave->pause_time;
			y = psave->pause_nrmsgs;
			psave->pause_time = psave->sig_pause_time;
			psave->pause_nrmsgs = psave->sig_pause_nrmsgs;
			psave->sig_pause_time = x;
			psave->sig_pause_nrmsgs = y;
			print_phrases(psave->msgs, suck_phrases[26], NULL);
		}
	}
}		
#endif
/*----------------------------------------------------------------*/
 int send_command(PMaster master, const char *cmd, char **ret_response, int good_response) {
	 /* this is needed so can do user authorization */

	 int len, retval = RETVAL_OK, nr;
	 char *resp;

	 if(master->debug == TRUE) {
		 do_debug("sending command: %s", cmd);
	 }
	 sputline(master->sockfd, cmd);
	 len = sgetline(master->sockfd, &resp);	
	 if( len < 0) {	
		 retval = RETVAL_ERROR;		  	
	 }					
	 else {
		 if(master->debug == TRUE) {
			 do_debug("got answer: %s", resp);
		 }

		 TimerFunc(TIMER_ADDBYTES, len, NULL);

		 number(resp, &nr);
		 if(nr == 480 ) { 
			 /* we must do authorization */
			 retval = do_authenticate(master);
			 if(retval == RETVAL_OK) {
				 /* resend command */
				 sputline(master->sockfd, cmd);
				 if(master->debug == TRUE) {
					 do_debug("sending command: %s", cmd);
				 }
				 len = sgetline(master->sockfd, &resp);
				 if( len < 0) {	
					 retval = RETVAL_ERROR;		  	
				 }
				 else {
					 number(resp,&nr);
					 if(master->debug == TRUE) {
						 do_debug("got answer: %s", resp);
					 }

					 TimerFunc(TIMER_ADDBYTES, len, NULL);

				 }
			 }
		 }
		 if (good_response != 0 && nr != good_response) {
			 error_log(ERRLOG_REPORT, suck_phrases[29],cmd,resp,NULL);
			 retval = RETVAL_UNEXPECTEDANS;
		 }
	 }
	 if(ret_response != NULL) {
		 *ret_response = resp;
	 }
	 return retval;
 }
/*------------------------------------------------------------------------------*/
/* 1. move N_OLDRC to N_OLD_OLDRC	N_OLDRC might not exist	       	        */
/* 2. move N_NEWRC to N_OLDRC							*/
/* 3. rm N_SUPPLEMENTAL								*/
/*------------------------------------------------------------------------------*/
void do_cleanup(PMaster master) {
	const char *oldptr;
	char ptr[PATH_MAX+1];
	struct stat buf;
	int exist;
	int okay = TRUE;

	if(master->debug == TRUE) {
		do_debug("checking for existance of suck.newrc\n");
	}
	
	if(stat(full_path(FP_GET, FP_TMPDIR, N_NEWRC), &buf) == 0) {
		if(master->debug == TRUE) {
			do_debug("found suck.newrc\n");
		}
		
		/* we do the above test, in case we were in a restart */
		/* with -R which would cause no suck.newrc to be created */
		/* since message_index() would be skipped */
		/* since no suck.newrc, don't move any of the sucknewrcs around */
		strcpy(ptr,full_path(FP_GET, FP_DATADIR, N_OLDRC));	
		/* must strcpy since full path overwrites itself everytime */
		oldptr = full_path(FP_GET, FP_DATADIR, N_OLD_OLDRC);

		/* does the sucknewsrc file exist ? */
		exist = TRUE;
		if(stat(ptr, &buf) != 0 && errno == ENOENT) {
			exist = FALSE;
		}
		if(master->debug == TRUE) {
			do_debug("sucknewsrc.old = %s sucknewsrc = %s exist = %s\n", oldptr, ptr, true_str(exist));
		}
		if(exist == TRUE && move_file(ptr, oldptr) != 0) {
			MyPerror(suck_phrases[30]);
			okay = FALSE;
		}
		else if(move_file(full_path(FP_GET, FP_TMPDIR, N_NEWRC), ptr) != 0) {
			MyPerror(suck_phrases[31]);
			okay = FALSE;
		}
	}
	else if( errno != ENOENT) {
		MyPerror(full_path(FP_GET, FP_DATADIR, N_NEWRC));
		okay = FALSE;
	}		
	if(okay == TRUE && unlink(full_path(FP_GET, FP_DATADIR, N_SUPPLEMENTAL)) != 0 && errno != ENOENT) {
		/* ENOENT is not an error since this file may not always exist */
		MyPerror(suck_phrases[33]);
	}
}
/*--------------------------------------------------------------------------------------*/
int scan_args(PMaster master, int argc, char *argv[]) {

	int loop, i, whicharg, arg, retval = RETVAL_OK;

	for(loop = 0 ; loop < argc && retval == RETVAL_OK ; loop++) {
		arg = ARG_NO_MATCH;
		whicharg = -1;
		
		if(master->debug == TRUE) {
			do_debug("Checking arg #%d-%d: '%s'\n", loop, argc, argv[loop]);
		}

		if(argv[loop][0] == FILE_CHAR) {
			/* totally skip these they are processed elsewhere */
			continue;
		}
		
		if(argv[loop][0] == '-' && argv[loop][1] == '-') {
			/* check long args */
			for(i = 0 ; i < NR_ARGS && arg == ARG_NO_MATCH ; i++) {
				if(arglist[i].larg != NULL) {
					if(strcmp(&argv[loop][2], arglist[i].larg) == 0) {
						arg = arglist[i].flag;
						whicharg = i;
					}
				}
			}
		}
		else if (argv[loop][0] == '-') {
			/* check short args */
			for(i = 0 ; i < NR_ARGS ; i++) {
				if(arglist[i].sarg != NULL) {
					if(strcmp(&argv[loop][1], arglist[i].sarg) == 0) {
						arg = arglist[i].flag;
						whicharg = i;
					}
				}
			}
		}
		if(arg == ARG_NO_MATCH) {
			/* we ignore the file_char arg, it is processed elsewhere */
			retval = RETVAL_ERROR;
			error_log(ERRLOG_REPORT, suck_phrases[34], argv[loop], NULL);
		}
		else {
			/* need to check for valid nr of args and feed em down */
			if(( loop + arglist[whicharg].nr_params) < argc) {
				retval = parse_args(master, arg, &(argv[loop+1]));
				/* skip em so they don't get processed as args */
				loop += arglist[whicharg].nr_params;
			}
			else {
				i = (arglist[whicharg].errmsg < 0) ? 34 : arglist[whicharg].errmsg;
				error_log(ERRLOG_REPORT, suck_phrases[i], NULL);
				retval = RETVAL_ERROR;
			}
		}
	}
	
	return retval;
}		 
/*---------------------------------------------------------------------------------------*/
int parse_args(PMaster master, int arg, char *argv[]) {

	int x, retval = RETVAL_OK;
	
	switch(arg) {
	case ARG_ALWAYS_BATCH:
		/* if we have downloaded at least one article, then batch up */
		/* even on errors */
		master->always_batch = TRUE;
		break;
/* batch file implies MultiFile mode */
	case  ARG_BATCH_INN: 
		master->batch = BATCH_INNXMIT;
		master->MultiFile = TRUE;
		master->batchfile = argv[0];
		break;
	case ARG_BATCH_RNEWS:
		master->batch = BATCH_RNEWS;
		master->MultiFile = TRUE;
		master->batchfile = argv[0];
		break;
	case ARG_BATCH_LMOVE:
		master->batch = BATCH_LMOVE;
		master->MultiFile = TRUE;
		master->batchfile = argv[0];
		break;
	case ARG_BATCH_INNFEED:
		master->batch = BATCH_INNFEED;
		master->MultiFile = TRUE;
		master->batchfile = argv[0];
		break;
	case ARG_BATCH_POST:
		master->batch = BATCH_LIHAVE;
		master->MultiFile = TRUE;
		master->batchfile = N_POSTFILE;
		break;
	case ARG_CLEANUP: 	/* cleanup option */
		master->cleanup = TRUE;
		break;
	case ARG_DIR_TEMP:
		full_path(FP_SET, FP_TMPDIR, argv[0]);
		break;
	case ARG_DIR_DATA:
		full_path(FP_SET, FP_DATADIR, argv[0]);
		break;
	case ARG_DIR_MSGS:
		full_path(FP_SET, FP_MSGDIR, argv[0]);
		break;
	case ARG_DEF_ERRLOG:	/* use default error log path */
		error_log(ERRLOG_SET_FILE, ERROR_LOG, NULL);
		master->errlog = ERROR_LOG;
		break;
	case ARG_HOST:	/* host name */
		master->host = argv[0];
		break;
	case ARG_NO_POSTFIX:	/* kill files don't use the postfix */	
		master->kill_ignore_postfix = TRUE;
		break;
	case ARG_LANGUAGE: 	/* load language support */
		master->phrases = argv[0];
		break;
	case ARG_MULTIFILE:
		master->MultiFile = TRUE;
		break;
	case ARG_POSTFIX:
		full_path(FP_SET_POSTFIX, FP_NONE, argv[0]);
		break;
	case ARG_QUIET: 	/* don't display BPS and message count */
		master->quiet = TRUE;
		break;
	case ARG_RNEWSSIZE:
		master->rnews_size = atol(argv[0]);
		break;
	case ARG_DEF_STATLOG:	/* use default status log name */
		master->status_file_name = STATUS_LOG;
		break;
	case ARG_WAIT_SIG:	/* wait (sleep) x seconds between every y articles IF we get a signal */
		master->sig_pause_time = atoi(argv[0]);
		master->sig_pause_nrmsgs = atoi(argv[1]);
		break;
	case ARG_ACTIVE:
		master->do_active = TRUE;
		break;
	case ARG_RECONNECT:	/* how often do we drop and reconnect to battle INNS's LIKE_PULLER=DONT */
		master->reconnect_nr = atoi(argv[0]);
		break;
	case ARG_DEBUG:	/* debug */
		master->debug = TRUE;
		/* now set up our error log and MyPerror reporting so it goes to debug.suck */
		error_log(ERRLOG_SET_DEBUG, NULL, NULL);
		break;
	case ARG_ERRLOG:	/* error log path */
		error_log(ERRLOG_SET_FILE, argv[0], NULL);
		master->errlog = argv[0];
		break;
	case ARG_HISTORY:
		master->do_chkhistory = FALSE;
		break;
	case ARG_KILLFILE:
		master->do_killfile = FALSE;
		break;
	case ARG_KLOG_NONE:
		master->killfile_log = KILL_LOG_NONE;
		break;
	case ARG_KLOG_SHORT:
		master->killfile_log = KILL_LOG_SHORT;
		break;
	case ARG_KLOG_LONG:
		master->killfile_log = KILL_LOG_LONG;
		break;
	case ARG_MODEREADER: /* mode reader */
		master->do_modereader = TRUE;
		break;
	case ARG_PORTNR:	/* override default portnr */
		master->portnr = atoi(argv[0]);
		break;
	case ARG_PASSWD:	/* passwd */
		master->passwd = argv[0];
		break;
	case ARG_RESCAN:	/* don't rescan on restart */
		master->rescan = FALSE;
		break;
	case ARG_STATLOG:	/* status log path */
		master->status_file_name = argv[0];
		break;
	case ARG_USERID:	/* userid */
		master->userid = argv[0];
		break;
	case ARG_VERSION: 	/* show version number */
		error_log(ERRLOG_REPORT,"Suck version %v1%\n",SUCK_VERSION, NULL);
		retval = RETVAL_VERNR;	/* so we don't do anything else */
		break;
	case ARG_WAIT:	/* wait (sleep) x seconds between every y articles */
		master->pause_time = atoi(argv[0]);
		master->pause_nrmsgs = atoi(argv[1]);
		break;
	case ARG_LOCALHOST:
		master->localhost = argv[0];
		break;
	case ARG_NRMODE:
		master->nrmode = TRUE;
		break;
	case ARG_AUTOAUTH:
		master->auto_auth = TRUE;
		break;
	case ARG_NODEDUPE:
		master->no_dedupe = TRUE;
		break;
	case ARG_READACTIVE:
		master->activefile = argv[0];
		break;
	case ARG_PREBATCH:
		master->prebatch = TRUE;
		break;
	case ARG_SKIP_ON_RESTART:
		master->skip_on_restart = TRUE;
		break;
	case ARG_KLOG_NAME:
		master->kill_log_name = argv[0];
		break;
	case ARG_USEGUI:
		master->use_gui = TRUE;
		break;
	case ARG_XOVER:
		master->do_xover = FALSE;
		break;
	case ARG_CONN_DEDUPE:
		master->conn_dedupe = TRUE;
		break;
	case ARG_POST_FILTER:
		master->post_filter = argv[0];
		break;
	case ARG_CONN_ACTIVE:
		master->conn_active = TRUE;
		break;
	case ARG_HIST_FILE:
		master->history_file = argv[0];
		break;
	case ARG_HEADER_ONLY:
		master->header_only = TRUE;
		break;
	case ARG_ACTIVE_LASTREAD:
		x = atoi(argv[0]);
		if(x > 0) {
			error_log(ERRLOG_REPORT, suck_phrases[66], NULL);
		}
		else {
			master->active_lastread = atoi(argv[0]);
		}
		break;
	case ARG_USEXOVER:
		master->use_xover = TRUE;
		break;
	case ARG_RESETCOUNTER:
		master->resetcounter = TRUE;
		break;
	case ARG_LOW_READ:
		master->low_read = TRUE;
		break;
	case ARG_SHOW_GROUP:
		master->show_group = TRUE;
		break;
#ifdef TIMEOUT
	case ARG_TIMEOUT:
		TimeOut = atoi(argv[0]);
		break;
#endif
	}
	
	return retval;

}
/*----------------------------------*/
/* authenticate when receive a 480 */
/*----------------------------------*/
int do_authenticate(PMaster master) {
	 int len, nr, retval = RETVAL_OK;
	 char *resp, buf[MAXLINLEN];

	 /* we must do authorization */
	 sprintf(buf, "AUTHINFO USER %s\r\n", master->userid);
	 if(master->debug == TRUE) {
		 do_debug("sending command: %s", buf);
	 }
	 sputline(master->sockfd, buf);
	 len = sgetline(master->sockfd, &resp);
	 if( len < 0) {	
	   retval = RETVAL_ERROR;		  	
	 }					
	 else {
		 if(master->debug == TRUE) {
			 do_debug("got answer: %s", resp);
		 }

		 TimerFunc(TIMER_ADDBYTES, len, NULL);

		 number(resp, &nr);
		 if(nr == 480) {
			 /* this is because of the pipelining code */
			 /* we get the second need auth */
			 /* just ignore it and get the next line */
			 /* which should be the 381 we need */
			 len = sgetline(master->sockfd, &resp);
			 TimerFunc(TIMER_ADDBYTES, len, NULL);
			 
			 number(resp, &nr);
		 }
		 
		 if(nr != 381) {
			 error_log(ERRLOG_REPORT, suck_phrases[27], resp, NULL);
			 retval = RETVAL_NOAUTH;
		 }
		 else {
			 sprintf(buf, "AUTHINFO PASS %s\r\n", master->passwd);
			 sputline(master->sockfd, buf);
			 if(master->debug == TRUE) {
				 do_debug("sending command: %s", buf);
			 }
			 len = sgetline(master->sockfd, &resp);
			 if(len < 0) {	
				 retval = RETVAL_ERROR;		  	
			 }					
			 else {
				 if(master->debug == TRUE) {
					 do_debug("got answer: %s", resp);
				 }

				 TimerFunc(TIMER_ADDBYTES, len, NULL);

				 number(resp, &nr);
				 switch(nr) {
	  			   case 281: /* bingo */
					 retval = RETVAL_OK;  
					 break;
				   case 502: /* permission denied */
					 retval = RETVAL_NOAUTH;
					 error_log(ERRLOG_REPORT, suck_phrases[28], NULL);
					 break;
				   default: /* wacko error */
					 error_log(ERRLOG_REPORT, suck_phrases[27], resp, NULL);
					 retval = RETVAL_NOAUTH;
					 break;
				 }
			 }
		 }
	 }
	 return retval;
 }
/*--------------------------------------------------------------------------------*/
/* THE strings in this routine is the only one not in the arrays, since           */
/* we are in the middle of reading the arrays, and they may or may not be valid.  */
/*--------------------------------------------------------------------------------*/
void load_phrases(PMaster master) {

	int error=TRUE;
	FILE *fpi;
	char buf[MAXLINLEN];

	if(master->phrases != NULL) {
		
		if((fpi = fopen(master->phrases, "r")) == NULL) {
			MyPerror(master->phrases);
		}
		else {
			fgets(buf, MAXLINLEN, fpi);
			if(strncmp( buf, SUCK_VERSION, strlen(SUCK_VERSION)) != 0) {
				error_log(ERRLOG_REPORT, "Invalid Phrase File, wrong version\n", NULL);
			}
			else if((both_phrases = read_array(fpi, NR_BOTH_PHRASES, TRUE)) != NULL) {
				read_array(fpi, NR_RPOST_PHRASES, FALSE);	/* skip these */
				read_array(fpi, NR_TEST_PHRASES, FALSE);
				if(( suck_phrases = read_array(fpi, NR_SUCK_PHRASES, TRUE)) != NULL &&
				   ( timer_phrases= read_array(fpi, NR_TIMER_PHRASES,TRUE)) &&
				   (  chkh_phrases= read_array(fpi, NR_CHKH_PHRASES,TRUE)) &&
				   (dedupe_phrases= read_array(fpi, NR_DEDUPE_PHRASES,TRUE)) &&
				   ( killf_reasons= read_array(fpi, NR_KILLF_REASONS,TRUE)) &&
				   ( killf_phrases= read_array(fpi, NR_KILLF_PHRASES,TRUE)) &&
				   ( sucku_phrases= read_array(fpi, NR_SUCKU_PHRASES,TRUE)) &&
				   (read_array(fpi, NR_LMOVE_PHRASES, FALSE) == NULL) && /* skip this one */
				   ( active_phrases=read_array(fpi, NR_ACTIVE_PHRASES,TRUE)) &&
				   ( batch_phrases= read_array(fpi, NR_BATCH_PHRASES,TRUE)) &&
				   ( xover_phrases= read_array(fpi, NR_XOVER_PHRASES,TRUE)) &&
				   ( xover_reasons= read_array(fpi, NR_XOVER_REASONS,TRUE)) &&
				   ( queue_phrases= read_array(fpi, NR_QUEUE_PHRASES,TRUE))) {
					error = FALSE;
				}
			}
			
		}
		fclose(fpi);
		if(error == TRUE) {
			/* reset back to default */
			error_log(ERRLOG_REPORT, "Using default Language phrases\n", NULL);
			both_phrases = default_both_phrases;
			suck_phrases=default_suck_phrases;
			timer_phrases=default_timer_phrases;
			chkh_phrases=default_chkh_phrases;
			dedupe_phrases=default_dedupe_phrases;
			killf_reasons=default_killf_reasons;
			killf_phrases=default_killf_phrases;
			killp_phrases=default_killp_phrases;
			sucku_phrases=default_sucku_phrases;
			active_phrases=default_active_phrases;
			batch_phrases=default_batch_phrases;
			xover_phrases=default_xover_phrases;
			xover_reasons=default_xover_reasons;
			queue_phrases=default_queue_phrases;
		}
	}		
}
/*--------------------------------------------------------------------------------*/
void free_phrases(void) {
		/* free up the memory alloced in load_phrases() */
		if(both_phrases != default_both_phrases) {
			free_array(NR_BOTH_PHRASES, both_phrases);
		}
		if(suck_phrases != default_suck_phrases) {
			free_array(NR_SUCK_PHRASES, suck_phrases);
		}
		if(timer_phrases != default_timer_phrases) {
			free_array(NR_TIMER_PHRASES, timer_phrases);
		}
		if(chkh_phrases != default_chkh_phrases) {
			free_array(NR_CHKH_PHRASES, chkh_phrases);
		}
		if(dedupe_phrases != default_dedupe_phrases) {
			free_array(NR_DEDUPE_PHRASES, dedupe_phrases);
		}
		if(killf_reasons != default_killf_reasons) {
			free_array(NR_KILLF_REASONS, killf_reasons);
		}
		if(killf_phrases != default_killf_phrases) {
			free_array(NR_KILLF_PHRASES, killf_phrases);
		}
		if(killp_phrases != default_killp_phrases) {
			free_array(NR_KILLP_PHRASES, killp_phrases);
		}
		if(sucku_phrases != default_sucku_phrases) {
			free_array(NR_SUCKU_PHRASES, sucku_phrases);
		}
		if(active_phrases != default_active_phrases) {
			free_array(NR_ACTIVE_PHRASES, active_phrases);
		}
		if(batch_phrases != default_batch_phrases) {
			free_array(NR_BATCH_PHRASES, batch_phrases);
		}
		if(xover_phrases != default_xover_phrases) {
			free_array(NR_XOVER_PHRASES, xover_phrases);
		}
		if(xover_reasons != default_xover_reasons) {
			free_array(NR_XOVER_REASONS, xover_reasons);
		}
		if(queue_phrases != default_queue_phrases) {
			free_array(NR_QUEUE_PHRASES, queue_phrases);
		}
		
		
}

		 
