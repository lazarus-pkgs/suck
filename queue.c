#include <config.h>

#ifdef DMALLOC
#include <dmalloc.h>
#endif

#include "suck_config.h"
#include "both.h"
#include "suck.h"
#include "queue.h"
#include "phrases.h"
#include "db.h"

#define QUEUE_SIZE 5
#define NR_SEND_AHEAD 2 
#define DEFAULT_HEADER_SIZE 8192  /* the start size of each header stored in memory */

typedef struct {
	char *header;
	long headersize;
} HdrQueueEntry, *PHdrQueueEntry;

typedef struct {
	int cmd;
	PList article;
	PGroups grp;
	HdrQueueEntry *hdr;
} CmdQueueEntry, *PCmdQueueEntry;

/* this queue holds the list of commands we've sent and are awaiting replies for */
typedef struct {
	int nr;
	int nron;
	CmdQueueEntry entry[QUEUE_SIZE];
} CmdQueue, *PCmdQueue;

/* this is a rotating queue of headers that we've received and are waiting the bodies for */
typedef struct {
	int nr;
	int nron;
	HdrQueueEntry entry[QUEUE_SIZE];
} HdrQueue, *PHdrQueue;

/* function prototypes */
int init_queue(PCmdQueue, PHdrQueue);
void free_queue(PCmdQueue, PHdrQueue);

enum {CMD_ARTICLE, CMD_HEADER, CMD_BODY, CMD_GROUP};


/*----------------------------------------------------------------------------------------*/
int get_queue(PMaster master) {
	int retval = RETVAL_OK;
	int done = FALSE;
	int cmd;
	
	PHdrQueueEntry hdr_got = NULL;
	
	CmdQueue cmds;
	HdrQueue hdrs;
	
	/* first, have to initialize queue */
	retval = init_queue(&cmds, &hdrs);
	if(retval == RETVAL_OK) {
		retval = db_open(master);
		if(retval == RETVAL_OK) {
			master->curr = master->head;
		}
	}
	
	while(retval == RETVAL_OK && done == FALSE) {
		while(cmds.nr < NR_SEND_AHEAD) {
			if(hdr_got != NULL) {
				/* we need to send the body command for the header we just got */
				cmd = CMD_BODY;
			}
			else if(master->curr != NULL) {
				cmd = (master->killp == NULL) ? CMD_ARTICLE : CMD_HEADER;
				
				/* send the appropriate command for this article */
			}
		}
		/* now that we've sent the commands, get the results off the socket */

		/* have we sent all the articles and received all the results? */
		if(master->curr == NULL && cmds.nr == 0) {
			done = TRUE;
		}
	}
	free_queue(&cmds, &hdrs);
	
	return retval;
}
/*----------------------------------------------------------------------------------------*/
/* initialize the cmdqueue and hdrqueue*/
int init_queue(PCmdQueue queue, PHdrQueue hdrs) {

	int retval = RETVAL_OK;
	int i;
	queue->nr = 0;
	queue->nron = 0;

	hdrs->nr = 0;
	hdrs->nron = 0;
	
	/* now set up the size of the header storage  */
	for (i = 0; i < QUEUE_SIZE; i++) {
		if((hdrs->entry[i].header = malloc(DEFAULT_HEADER_SIZE)) == NULL) {
			retval = RETVAL_ERROR;
			error_log(ERRLOG_REPORT, queue_phrases[0], NULL);
		}
		else{
			hdrs->entry[i].headersize = DEFAULT_HEADER_SIZE;
		}
	}
	return retval;
}
/*-------------------------------------------------------------------------------------*/
void free_queue(PCmdQueue queue, PHdrQueue hdrs) {
	int i;

	for(i = 0;i < QUEUE_SIZE; i++) {
		if(hdrs->entry[i].header != NULL) {
			free(hdrs->entry[i].header);
		}
	}
}
/*-------------------------------------------------------------------------------------*/

