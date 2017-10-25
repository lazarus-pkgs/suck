#include <config.h>

/* If we don't use history databases, then these routines will */
/* get compiled and used.  If we do, the routines in chkhistory_db.c will */
/* be used. */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef DMALLOC
#include <dmalloc.h>
#endif

#include "suck_config.h"
#include "suck.h"
#include "both.h"
#include "chkhistory.h"
#include "suckutils.h"
#include "phrases.h"
#include "timer.h"
#include "ssort.h"

/* this is almost a duplicate of dedupe_list() */
/* put items in array, qsort it, then work thru history file, and use bsearch to match em */
void chkhistory(PMaster master) {
	FILE *fhist;
	PList *array, ptr, curr, prev, tptr;
	int nrin=0, nrfound=0;
	long valinhist = 0, nrinhist = 0;
	char *cptr, linein[MAXLINLEN+1];

	if((fhist = fopen(master->history_file, "r")) == NULL) {
		MyPerror(master->history_file);
	}
	else {
		if(master->debug == TRUE) {
			do_debug("Chking %d master->nritems\nReading history file - %s\n",
				 master->nritems, master->history_file);
		}
		print_phrases(master->msgs, chkh_phrases[1], NULL);
		fflush(master->msgs);	/* so msg gets printed */

		TimerFunc(TIMER_START, 0L, NULL);

		/* 1. throw em into the array */
		if((array = calloc(master->nritems, sizeof(PList))) == NULL) {
			error_log(ERRLOG_REPORT, chkh_phrases[4], NULL);
		}
		else {
			ptr = master->head;
			nrin = 0;
			while(ptr != NULL) {
				array[nrin] = ptr;
				nrin++;
				ptr = ptr->next;
			}
			if(master->debug == TRUE) {
				do_debug("Added %d items to array for ssort, master->nritems = %d\n", nrin, master->nritems);
			}
			/* step 2, sort em */
			ssort(array, nrin, 1); /* start with depth of 1 cause all start with < */

			/* step 3, find and mark dupes */
			/* history file can have two valid types of lines */
			/* the ones we are interested in is the ones that start with < */
			/* we ignore the ones that start with [  and report the rest */
			while(fgets(linein, MAXLINLEN, fhist) != NULL) {
				nrinhist++;
				if(linein[0] == '<') {
					/* make sure we have valid msgid and null terminate it */
					if((cptr = strchr(linein, '>'))  == NULL) {
						error_log(ERRLOG_REPORT, chkh_phrases[2], linein, NULL);
					}
					else {
						valinhist++;
						*(cptr+1) = '\0';
						if((tptr = my_bsearch(array, linein, nrin)) != NULL) {
							/* found in our array flag for deletion */
							/* it returns a PList * */
							tptr->delete = TRUE;
							nrfound++;
						}
					}
				}
			}
			if(ferror(fhist) != 0) {
				error_log(ERRLOG_REPORT, chkh_phrases[6], master->history_file, NULL);
				clearerr(fhist);
			}

			if(master->debug == TRUE) {
				do_debug("%d lines in history, %d valid lines, %d duplicates found\n", nrinhist, valinhist, nrfound);
			}
			/* step 4, delete em */
			curr = master->head;
			prev = NULL;
			while(curr != NULL) {
				if( curr->delete == TRUE) {
				/* nuke it */
					master->nritems--;
					if(prev == NULL) {
						/* remove master node */
						master->head = curr->next;
						free_one_node(curr);
						curr = master->head;
					}
					else {
						prev->next = curr->next;
						free_one_node(curr);
						curr = prev->next;
					}
				}
				else {
					prev = curr;
					curr = curr->next;
				}
			}

			/* all done free up mem */
			free(array);
		}
		fclose(fhist);
		TimerFunc(TIMER_TIMEONLY, 0l, master->msgs);
		print_phrases(master->msgs, chkh_phrases[3], str_int(nrfound), NULL);
	}
}

