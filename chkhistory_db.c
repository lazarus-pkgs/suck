#include <config.h>

/* If we use history databases, then these routines will */
/* get compiled and used.  If we don't, the routines in chkhistory.c will */
/* be used. */

#include <stdio.h>

#include "suck_config.h"
#include "suck.h"
#include "both.h"
#include "chkhistory.h"
#include "suckutils.h"
#include "phrases.h"
#include "timer.h"

/* These take care if user had multiple defines in makefile */
/* use DBM if we've got it */
#ifdef USE_DBM
#undef USE_GDBM
#undef USE_NDBM
#undef USE_DBZ
#undef USE_INN2
#undef USE_INN23
#endif
/* else if have GDBM use it */
#ifdef USE_GDBM
#undef USE_NDBM
#undef USE_DBZ
#undef USE_INN2
#undef USE_INN23
#endif
/* else if have NDBM use it */
#ifdef USE_NDBM
#undef USE_DBZ
#undef USE_INN2
#undef USE_INN23
#endif
/* use DBZ if we have it */
#ifdef USE_DBZ
#undef USE_INN2
#undef USE_INN23
#endif
/* use INN2 if we have it */
#ifdef USE_INN2
#undef USE_INN23
#endif

#ifdef USE_DBM
#include <dbm.h>
#define close_history() dbmclose();
#endif

#ifdef USE_GDBM
#include <gdbm.h>
static GDBM_FILE dbf = NULL;
#define close_history() gdbm_close(dbf)
#endif

#ifdef USE_NDBM
#include <ndbm.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
static DBM *db = NULL;	/* I know this isn't too pretty, but its the easiest way to do it */
#define close_history() dbm_close(db)
#endif

#ifdef USE_DBZ
#include <dbz.h>
#define close_history() dbmclose()
#endif

#ifdef USE_INN2
#include <sys/types.h>
#include <configdata.h>
#include <clibrary.h>
#include <libinn.h>
#include <dbz.h>
#define close_history() dbzclose()
#endif
#ifdef USE_INN23
#include <sys/types.h>
#ifndef NO_CONFIGDATA
#include <configdata.h>
#endif
#include <libinn.h>
#include <dbz.h>
#define close_history() dbzclose()
#define USE_INN2    /* we need the rest of the inn2 code, only the includes are different */
#endif

#ifdef DMALLOC
#include <dmalloc.h>
#endif


/* function prototypes */
int open_history(const char *);
int  check_history(char *);
/*---------------------------------------------------------------------------------------------------*/
void chkhistory(PMaster master) {

	PList curr, prev;
	int nrfound = 0;

	if(master->debug == TRUE) {
		do_debug("Opening history database: %s\n", master->history_file);
	}
	
	if(open_history(master->history_file) != TRUE) {
		error_log(ERRLOG_REPORT, chkh_phrases[0], master->history_file, NULL);
	}
	else {

		print_phrases(master->msgs, chkh_phrases[5], NULL);
		fflush(master->msgs);	/* so msg gets printed */
		TimerFunc(TIMER_START, 0L, NULL);

		/* okay cycle thru our list, checking each against the history DB */
		curr = master->head;
		prev = NULL;
		
		while(curr != NULL ) {
			if(check_history(curr->msgnr) == TRUE) {
				if(master->debug == TRUE) {
					do_debug("Matched %s, nuking\n", curr->msgnr);
				}
				/* matched, nuke it */
				nrfound++;
				master->nritems--;
				if(prev == NULL) {
					/* remove master node */
					master->head = curr->next;
					free_one_node(curr);
					curr = master->head;	/* next node to check */
				}
				else {
					prev->next = curr->next;
					free_one_node(curr);
					curr = prev->next;	/* next node to check */
				}
			}
			else {
				if(master->debug == TRUE) {
					do_debug("Didn't match %s\n", curr->msgnr);
				}
				/* next node to check */
				prev = curr;
				curr = curr->next;
			}
		}
		TimerFunc(TIMER_TIMEONLY, 0l, master->msgs);

		close_history();
		print_phrases(master->msgs, chkh_phrases[3], str_int(nrfound), NULL);
	}
	return;
}
/*------------------------------------------------------------------------*/
int open_history(const char *history_file) {
	
	int retval = FALSE;

#if defined (USE_DBM) || defined (USE_DBZ)

	if(dbminit(history_file) >= 0) {
		retval = TRUE;
	}
#endif

#if defined (USE_INN2) 
	/* first, set our options */
	dbzoptions opt;

#ifdef DO_TAGGED_HASH
	opt.pag_incore=INCORE_MEM;
#else
#ifndef USE_INN23
	opt.idx_incore=INCORE_MEM;  /* per man page speeds things up, inn-2.3 doesn't need this */
#endif	
#endif
	opt.exists_incore=INCORE_MEM;
	dbzsetoptions(opt);
	retval = dbzinit(history_file);
#endif
	
#if defined (USE_NDBM)
	if((db = dbm_open(history_file, O_RDONLY, 0)) != NULL) {
		retval = TRUE;
	}
#endif
#if defined(USE_GDBM)
	if((dbf = gdbm_open(history_file, 1024, GDBM_READER,0,0)) != NULL) {
		retval = TRUE;
	}
#endif

	return retval;
}
/*----------------------------------------------------------------*/
int check_history(char *msgid) {

#if defined (USE_DBM) || defined (USE_DBZ) || defined (USE_NDBM)
	datum input, result;

	input.dptr = msgid;
	input.dsize = strlen(msgid) +1;

#if defined (USE_DBM)
	
	result = fetch(input);

#elif defined (USE_DBZ) 

	result = dbzfetch(input);

#elif defined (USE_NDBM)

	result = dbm_fetch(db, input);

#endif

	return (result.dptr != NULL) ? TRUE : FALSE;
#endif /* big if defined */

#if defined (USE_INN2)
	HASH msgid_hash;
	
	msgid_hash=HashMessageID(msgid);	/* have to hash it first */
	return dbzexists(msgid_hash);
	
#endif
#if defined(USE_GDBM)
	datum input;

	input.dptr = msgid;
	input.dsize = strlen(msgid)+1;

	return gdbm_exists(dbf, input);
#endif	
}		
