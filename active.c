#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_REGEX_H
#include <regex.h>
#endif

#ifdef DMALLOC
#include <dmalloc.h>
#endif

#include "suck_config.h"
#include "both.h"
#include "suck.h"
#include "phrases.h"
#include "timer.h"
#include "suckutils.h"
#include "active.h"

typedef struct Dactive {
	char *group;
	int high;
	int done;		/* have we gotten the msgids ? */
	int postyn;
	struct Dactive *next;
} Active, *Pactive;

typedef struct Dignore {
	char *group;
	struct Dignore *next;
#ifdef HAVE_REGEX_H
	regex_t match;
	int use_regex;
#endif
} Ignore, *Pignore;

/* local function prototypes */
int add_to_list(Pactive *, const char *, Pignore, int);
int get_msgids(PMaster, Pactive);
Pignore  read_ignore(PMaster);
void do_free(Pactive, Pignore);
Pactive nntp_active(PMaster, Pignore);
Pactive read_active(PMaster, Pignore);

/*-------------------------------------------------------------------------*/
int get_message_index_active(PMaster master) {
	
	int retval = RETVAL_OK;
	Pactive listhead = NULL;
	Pignore ignorehead = NULL;

	TimerFunc(TIMER_START, 0, NULL);
	
	/* first get our groups to ignore */
	ignorehead = read_ignore(master);

	/* Now try to get the active list */
	if(master->do_active == TRUE) {
		listhead = nntp_active(master, ignorehead);
	}
	if(listhead == NULL && master->activefile != NULL) {
		/* can we get active list from local file */
		listhead = read_active(master, ignorehead);
	}
	/* if it takes a while to read the active the other end */
	/* might have timed out, hence this option */
	if(master->conn_active == TRUE) {
		retval = do_connect(master, CONNECT_AGAIN);
	}
	/* now get the MsgIds */
	if(retval == RETVAL_OK && listhead != NULL) {
		retval = get_msgids(master, listhead);
		do_free(listhead, ignorehead);
	}
	else {
		retval = RETVAL_ERROR;
	}
	TimerFunc(TIMER_TIMEONLY, 0, master->msgs);

	if(retval == RETVAL_ERROR){
		retval = get_message_index(master);
	}
	else {
		print_phrases(master->msgs, active_phrases[5], str_int(master->nritems), NULL);
	}
	
	return retval;
}
/*------------------------------------------------------------------------*/
Pactive nntp_active(PMaster master, Pignore ignorehead) {

	int localfd, nr;
	char *resp;
	Pactive listhead = NULL;
	int retval = RETVAL_OK;
	
	/* get active file from nntp host */
	if((localfd = connect_local(master)) < 0 ) {
		error_log(ERRLOG_REPORT, active_phrases[0], NULL);
	}
	else {
		/* now get the list of groups */
		print_phrases(master->msgs, active_phrases[2], master->localhost, NULL);
		
		if(master->debug == TRUE) {
			do_debug("Sending command: LIST\n");
		}
		sputline(localfd, "LIST\r\n", master->local_ssl, master->local_ssl_struct);
		if(sgetline(localfd, &resp, master->local_ssl, master->local_ssl_struct) >= 0) {
			if(master->debug == TRUE) {
				do_debug("got answer: %s", resp);
			}
			number(resp, &nr);
			if(nr == 215) { /* now we can get the list */
				do {
					if(sgetline(localfd, &resp, master->local_ssl, master->local_ssl_struct) > 0) {
						if(master->debug == TRUE) {
							do_debug("Got groupline: %s", resp);
						}
						if(resp[0] != '.' ) {	
							retval = add_to_list(&listhead, resp, ignorehead,master->debug);
						}
					}
					else {
						retval = RETVAL_ERROR;
					}		
				}
				while(resp[0] != '.' && retval == RETVAL_OK);
				if(retval != RETVAL_OK) {
					do_free(listhead, NULL);
					listhead = NULL;
				}
			}
		}
		disconnect_from_nntphost(localfd, master->local_ssl, &master->ssl_struct);
	}
	return listhead;
}
/*------------------------------------------------------------------------*/
Pactive read_active(PMaster master, Pignore ignorehead) {

	/* read active list from file */
	Pactive listhead = NULL;
	FILE *fpi;
	char linein[MAXLINLEN];
	int retval = RETVAL_OK;

	if(master->debug == TRUE) {
		do_debug("Opening Active file: %s\n", master->activefile);
	}
	if((fpi = fopen(master->activefile, "r")) == NULL) {
		error_log(ERRLOG_REPORT, active_phrases[11], master->activefile, NULL);
	}
	else {
		while(fgets(linein, MAXLINLEN, fpi) != NULL && retval == RETVAL_OK) {
			if(master->debug == TRUE) {
				do_debug("Got line: %s", linein);
			}
			
			retval = add_to_list(&listhead, linein, ignorehead, master->debug);
		}
		fclose(fpi);
		if(retval != RETVAL_OK) {
			do_free(listhead, NULL);
			listhead = NULL;
		}
	}
	return listhead;
	
}
/*------------------------------------------------------------------------*/
int connect_local(PMaster master) {
	
	/* connect to localhost NNTP server */
	int fd;
	char *inbuf;
	unsigned int port;

	port = (master->local_ssl == TRUE) ? LOCAL_SSL_PORT : LOCAL_PORT;
	if(master->debug == TRUE) {
		do_debug("Connecting to %s on port %d\n", master->localhost, port);
	}
	
	if((fd = connect_to_nntphost(master->localhost, NULL, 0, NULL, port, master->local_ssl, &master->local_ssl_struct)) >= 0) {
		/* get the announcement line */
		if(sgetline(fd, &inbuf, master->local_ssl, master->local_ssl_struct) < 0) {
			close(fd);
			fd = -1;
		}
		else if(master->debug == TRUE) {
			do_debug("Got: %s", inbuf);
		}
	}	
	return fd;
}
/*----------------------------------------------------------------------------*/
int add_to_list(Pactive *head, const char *groupline, Pignore ignorehead, int debug) {
	
	/* add one group to group list */
	Pactive temp, tptr;
	Pignore pignore;
	int len, retval= RETVAL_OK;
	char postyn;
	
#ifdef HAVE_REGEX_H
	int reg_match;
#endif
	
	len = 0;
	/* get length of group name */
	while(groupline[len] != ' ' && groupline[len] != '\0') {
		len++;
	}
	if((temp = malloc(sizeof(Active))) == NULL) {
		error_log(ERRLOG_REPORT, active_phrases[1], NULL);
		retval = RETVAL_ERROR;
	}
	else if((temp->group = malloc(len+1)) == NULL) {
		error_log(ERRLOG_REPORT, active_phrases[1], NULL);
		retval = RETVAL_ERROR;
	}
	else {
		/* now initialize the sucker and add it to the list*/
		temp->next = NULL;
		temp->high = 0;
		temp->done = FALSE;
		strncpy(temp->group, groupline, len);
		temp->group[len] = '\0'; /* NULL terminate it */
		
		sscanf(groupline, "%*s%*ld%*ld%c", &postyn);
		temp->postyn = (postyn == 'n') ? FALSE : TRUE;

		/* now check to see if we ignore this group, if so, don't add to list */
		/* we have to wait until here, because only now do we have the group */
		/* name in a separate field we can compare to the ignore list */
		pignore = ignorehead;
#ifndef HAVE_REGEX_H
		while(pignore != NULL && strcmp(pignore->group, temp->group) != 0) {
			pignore = pignore->next;
		}
#else
		reg_match = FALSE;
		
		while(pignore != NULL && reg_match == FALSE) {
			if(pignore->use_regex == FALSE) {
				if(strcmp(pignore->group, temp->group) == 0) {
					reg_match = TRUE;
				}
				else {
					pignore = pignore->next;
				}
			}
			else {
				if(regexec(&(pignore->match),temp->group, 0, NULL, 0) == 0) {
					reg_match = TRUE;
				}
				else {
					pignore = pignore->next;
				}
			}
		}
#endif
		if(debug == TRUE ) {
			if(pignore == NULL) {
				do_debug("Adding to active list - %s\n",temp->group);
			}
			else {
				do_debug("Ignoring Group %s - match on %s\n",temp->group, pignore->group);
			}
		}
		if(pignore == NULL) {
			/* we didn't match, add to list */
			if(*head == NULL) {
				/* head node */
				*head = temp;
			}
			else {
				/* find end of list */
				tptr = *head;
				while(tptr->next != NULL) {
					tptr = tptr->next;
				}
				tptr->next = temp;
			}
		}
		
	}	
	return retval;
}
/*---------------------------------------------------------------------------------*/
void do_free(Pactive head, Pignore ihead) {

	/* free both linked lists and any alloced strings */
	
	Pactive temp;
	Pignore itemp;
	
	while(head != NULL) {
		if(head->group != NULL) {
			free(head->group);
		}
		
		temp=head->next;
		free(head);
		head = temp;
	}
	while(ihead != NULL) {
		if(ihead->group != NULL) {
			free(ihead->group);
		}
		itemp = ihead->next;
		free(ihead);
		ihead = itemp;
	}
	
}
/*-------------------------------------------------------------------------------*/
int get_msgids(PMaster master, Pactive head) {

	/* read in the sucknewsrc, check to see if group is in active list */
	/* then download msgids and write it out to the new.sucknewsrc */
	
	FILE *oldrc, *newrc;
	int retval = RETVAL_OK, nrread, maxread;
	long lastread;
	char buf[MAXLINLEN+1], group[512], *ptr;
	Pactive plist;
	
	oldrc = newrc = NULL;


	if((newrc = fopen(full_path(FP_GET, FP_TMPDIR, N_NEWRC), "w" )) == NULL) {
		MyPerror(full_path(FP_GET, FP_TMPDIR, N_NEWRC));
		retval = RETVAL_ERROR;
	}
	
	if((oldrc = fopen(full_path(FP_GET, FP_DATADIR, N_OLDRC), "r" )) == NULL) {
		/* this isn't actually an error, since we can create it */
		print_phrases(master->msgs, active_phrases[6], NULL);	
	}
	else {
		print_phrases(master->msgs, active_phrases[9], NULL);
		
		while(retval == RETVAL_OK && fgets(buf, MAXLINLEN-1, oldrc) != NULL) {
			ptr = buf;
			if(*ptr == SUCKNEWSRC_COMMENT_CHAR) {
				/* skip any white space before newsgroup name */
				while(! isalpha(*ptr)) {
					ptr++;
				}
			
			}
			maxread = -1; /* just in case */
			nrread = sscanf(ptr, "%s %ld %d\n", group, &lastread, &maxread);
			if(nrread < 2 || nrread > 3) {
				/* totally ignore any bogus lines */
				print_phrases(master->msgs, active_phrases[3], buf, NULL);
			}
			else {
				/* now find if group is still in active or not */
				plist = head;
				while( plist != NULL && strcmp(plist->group, group) != 0) {
					plist = plist->next;
				}
				if(plist == NULL) {
					print_phrases(master->msgs, active_phrases[4], buf, NULL);
				}
				else {
				/* valid group, lets get em */
					if(plist->postyn == FALSE) {
						/* we can't post, comment the line out */
						fprintf(newrc, "# %s %ld", group, lastread);
						if(maxread >= 0) {
							fprintf(newrc, " %d", maxread);
						}
						fputc('\n', newrc);
					}
					else if(maxread == 0) {
						/* just rewrite the line */
						fprintf(newrc,"%s %ld %d\n", group, lastread, maxread);
					}
					else {
						retval = do_one_group(master, buf, group, newrc, lastread, maxread);
						plist->done = TRUE;
					}
				}
			}	
		}

                /* this is in case we had to abort the above while loop (due to loss of pipe to server) */
		/* and we hadn't finished writing out the suck.newrc, this finishes it up. */
		if(retval != RETVAL_OK) {
			do {
				fputs(buf, newrc);
			}
			while(fgets(buf, MAXLINLEN-1, oldrc) != NULL);
		}
		
		fclose(oldrc);
	}
	
	if(retval == RETVAL_OK) {
		/* okay add any new groups from active that weren't already in sucknewsrc */
		plist = head;
		if(plist != NULL) {
			print_phrases(master->msgs, active_phrases[8], NULL);
		}
		
		while( plist != NULL) {
			if(plist->done == FALSE) {
				/* create a line for newsrc file */
				lastread = master->active_lastread;
				maxread = 0;
				
				sprintf(buf,"%s %ld\n", plist->group, lastread);

				print_phrases(master->msgs, active_phrases[10], plist->group, NULL);
				
				if(plist->postyn == FALSE) {
					/* we can't post, comment the line out */
					fprintf(newrc, "# %s", buf);
				}
				else {
					retval = do_one_group(master, buf, plist->group, newrc, lastread, maxread);
					plist->done = TRUE;
				}			
			}
			plist = plist->next;
		}	
	}
	if(newrc != NULL) {
		fclose(newrc);
	}
	
	return retval;
}
/*--------------------------------------------------------------------------------------------------*/
Pignore read_ignore(PMaster master) {


	/* read in ignore file, and build list of groups to ignore */
	FILE *fpi;
	Pignore head, temp, last;
	char buf[MAXLINLEN+1], *ptr;
	int errflag = FALSE;
	
#ifdef HAVE_REGEX_H
	int err;
	char errmsg[256];
	int i;
#endif

	head = last = NULL;
	
	/* first check if one with postfix is present, if not, use non postfix version */
	fpi = fopen(full_path(FP_GET, FP_DATADIR, N_ACTIVE_IGNORE), "r");
	if(fpi == NULL) {
		fpi = fopen(full_path(FP_GET_NOPOSTFIX, FP_DATADIR, N_ACTIVE_IGNORE), "r");
	}
	
	if(fpi != NULL) {
		while(fgets(buf, MAXLINLEN, fpi) != NULL) {
			/* strip off any trailing spaces from group name */
			ptr = buf;
			while(!isspace(*ptr)) {
				ptr++;
			}
			*ptr = '\0';
			
			if((temp = malloc(sizeof(Ignore))) == NULL) {
				error_log(ERRLOG_REPORT, active_phrases[7], NULL);
				errflag = TRUE;
			}
			else if((temp->group = malloc(strlen(buf)+1)) == NULL) {
				error_log(ERRLOG_REPORT, active_phrases[7], NULL);
				errflag = TRUE;
			}
			else {
				if(master->debug == TRUE) {
					do_debug("Ignoring group %s\n", buf);
				}
				
				/* add to list */
				strcpy(temp->group, buf);
				temp->next = NULL;
				if(head == NULL) {
					head = temp;
				}
				else {
					last->next = temp;
				}
				last = temp;
#ifdef HAVE_REGEX_H
				/* first add ^ and $ so  that we don't match */
				/* partials unless they have wild cards */
				if(buf[0] != '^') {
					buf[0] = '^';
					strcpy(&buf[1], temp->group);
				}
				else {
					strcpy(buf,temp->group);
				}
				i = strlen(buf);
				if(buf[i-1] != '$') {
					buf[i] = '$';
					buf[i+1] = '\0';
				}
				
                                /* now regcomp it for later comparision */
				temp->use_regex = TRUE;
				if((err = regcomp(&(temp->match), buf, REG_NOSUB | REG_ICASE | REG_EXTENDED)) != 0) 
				{
					regerror(err, &(temp->match), errmsg, sizeof(errmsg));
					error_log(ERRLOG_REPORT, active_phrases[12], buf, errmsg, NULL);
					temp->use_regex = FALSE;
				}
				
#endif
			}
		}
		fclose(fpi);
	}
	if(errflag == TRUE) {
		while(head != NULL) {
			if(head->group != NULL) {
				free(head->group);
			}
			temp = head->next;
			free(head);
			head = temp;
		}
	}
	return head;
}

