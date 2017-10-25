#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#ifdef DMALLOC
#include <dmalloc.h>
#endif

#include "suck_config.h"
#include "suck.h"
#include "both.h"
#include "dedupe.h"
#include "suckutils.h"
#include "phrases.h"
#include "timer.h"
#include "ssort.h"

/* this is almost a duplicate of chkhistory() routine. */

void dedupe_list(PMaster master) {
	/* throw the items into an array, qsort em, and then just compare items in a row to dedupe */

	PList *array, ptr, curr, prev;
	int i=0, nrfound=0;

	print_phrases(master->msgs, dedupe_phrases[0], NULL);
	fflush(master->msgs);	/* so msg gets printed */

	TimerFunc(TIMER_START, 0L, NULL);

	/* 1. throw em into the array */
	if((array = calloc(master->nritems, sizeof(PList))) == NULL) {
		error_log(ERRLOG_REPORT, dedupe_phrases[1], NULL);
	}
	else {
		ptr = master->head;
		i = 0;
		while(ptr != NULL) {
			array[i] = ptr;
			i++;
			ptr = ptr->next;
		}

		TimerFunc(TIMER_START, 0L, NULL) ;

		/* step 2, sort em */
		ssort(array, master->nritems, 1);  /* start with depth of 1 cause all start with < */

		/* TimerFunc(TIMER_TIMEONLY, 0L, master->msgs); */


		/* step 3, mark dupes */
		for(i=0;i<(master->nritems-1);i++) {
			if(cmp_msgid(array[i]->msgnr,array[i+1]->msgnr) == TRUE) {
				/* if this is one we've already downloaded, or one we've already marked as dupe */
				/* delete the other one */
				if(array[i]->delete == TRUE || array[i]->downloaded == TRUE) {
					array[i+1]->delete = TRUE;
				}
				else {
					array[i]->delete = TRUE;
				}

				nrfound++;
			}
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
	TimerFunc(TIMER_TIMEONLY, 0l, master->msgs);

	print_phrases(master->msgs, dedupe_phrases[2], str_int(master->nritems),  str_int(nrfound), NULL);
}

