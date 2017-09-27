#include <config.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#ifdef DMALLOC
#include <dmalloc.h>
#endif

#include "suck_config.h"
#include "suck.h"
#include "both.h"
#include "xover.h"
#include "phrases.h"
#include "killfile.h"
#include "suckutils.h"

#ifdef EMX
#define strcasecmp(x,y) strcmp((x),(y))
#endif

typedef struct Grplist {
	PGroup grp;
	struct Grplist *next;
} GrpList, *PGrpList;

/*func prototypes */
int do_one_line(PMaster, char *, char *,PGrpList, int);
int chk_a_group(PMaster, POneKill, POverview, char **);
int match_group(char *, char *, int);
char *find_msgid(PMaster, char *);

/* these MUST match xover_reasons[] ! EXCEPT FOR THE last entry which MUST BE REASON_NONE */
enum { REASON_HIBYTES, REASON_LOWBYTES, REASON_HILINES, REASON_LOWLINES, REASON_HEADER, REASON_NOKEEP, REASON_TIE,\
       REASON_XREF, REASON_PRG, REASON_PERL, REASON_NONE};

/*----------------------------------------------------------------------*/
int do_group_xover(PMaster master, char *grpname, long startnr, long endnr) {

	int done, len, nr, retval = RETVAL_OK;
	char *resp, cmd[MAXLINLEN];
	PKillStruct xoverkill;
	PGroup grpkill = NULL;
	PGrpList glist = NULL, gat = NULL, gptr;
	
	sprintf(cmd, "xover %ld-%ld\r\n", startnr,endnr);
	retval = send_command(master, cmd, &resp, 0);
	if(retval == RETVAL_OK) {
		number(resp, &nr);
		switch(nr) {
		  default:
		  case 412:
		  case 502:
			  /* abort on these errors */
			  error_log(ERRLOG_REPORT, xover_phrases[0], resp, NULL);
			  retval = RETVAL_NOXOVER;
			  break;
		  case 420:
			  /* no articles available, no prob */
			  break;
		  case 224:
			  /* first, check to see if there are group kill file(s) for this group */
			  /* we do this here, so we only do it once per group, since we don't */
			  /* have all the crossposted groups to deal with */
			  xoverkill = master->xoverp;
			  if(xoverkill->totgrps > 0) {
				  for(nr = 0; xoverkill != NULL && nr < xoverkill->totgrps; nr++) {
					  if(match_group(xoverkill->grps[nr].group, grpname, master->debug) == TRUE) {
						  grpkill = &(xoverkill->grps[nr]);
						  if(master->debug == TRUE) {
							  do_debug("Using Group Xover killfile %s\n", grpkill->group);
						  }
						  /* now add it to our list to pass down to do_one_line()*/
						  if((gptr = calloc(1, sizeof(GrpList))) == NULL) {
							  error_log(ERRLOG_REPORT, xover_phrases[12], NULL);
							  retval = RETVAL_ERROR;
						  }
						  else {
							  gptr->grp = grpkill;
							  gptr->next = NULL;
							  /* add to list */
							  if(glist == NULL) {
								  /* head of list */
								  glist = gptr;
							  }
							  else {
								  gat->next = gptr;
							  }
							  gat = gptr;
						  }
					  }
				  }
			  }
			  
			  /* okay, do it */
			  done = FALSE;
			  while((done == FALSE) && (retval == RETVAL_OK)) {
				  len = sgetline(master->sockfd, &resp);
				  /* have we got the last one? */
				  if((len == 2) && (strcmp(resp, ".\n") == 0)) {
					  done = TRUE;
				  }
				  else if(len < 0) {
					  retval =RETVAL_ERROR;
				  }
				  else {
					  retval = do_one_line(master, grpname, resp, glist, xoverkill->tie_delete);
				  }
			  }
			  break;
		}
	}
	while(glist != NULL) {
		/* free up memory */
		gptr = glist->next;
		free(glist);
		glist = gptr;
	}
	
	return retval;
}
/*-----------------------------------------------------------------------------------*/
int do_one_line(PMaster master, char *group, char *linein, PGrpList grpskill, int tie_delete) {

	/* take in one line of xover, then run it thru the kill routines */
	
	int why = REASON_NONE, len, msgidlen = 0, retval = RETVAL_OK, match = REASON_NONE, keep = FALSE, del = FALSE;
	char *msgid = NULL, *ptr, *reason = NULL, tmp = '\0';
	long msgnr;
	POverview overv;
	PGroup grpkill;
	
	PKillStruct masterk = master->xoverp; /* our master killstruct */
	overv = master->xoverview;
	ptr  = get_long(linein, &msgnr);  /* get our message number */
	
	/* first go thru and match up the header with the fields in the line, and fill */
	/* in the pointers in the xoverview with pointers to start of field */
	while( *ptr != '\0' && overv != NULL) {
		/* we are just past a tab */
		len = 0;
		if(overv->full == TRUE) {
			/* have to skip header name in the xoverview */
			while(*ptr != COLON && *ptr != '\0' && *ptr != '\t') {
				ptr++;
			}
			if(*ptr != '\0') {
				ptr++; /* move past colon */
			}
		}
		overv->field = ptr;
		while(*ptr != '\t' && *ptr != '\0') {
			len++;
			ptr++;
		}
		overv->fieldlen = len;
		
		/* save Message-ID location for later passing to subroutines */
		if(strcasecmp("Message-ID:", overv->header) == 0) {
			msgid = overv->field;
			msgidlen = overv->fieldlen;
		}
		overv = overv->next;
		if(*ptr == '\t') {
			ptr++; /* advance past tab */
		}
	}
	while(overv != NULL) {
		/* in case we got a short xoverview */
		overv->field = NULL;
		overv->fieldlen = 0;
		overv = overv->next;
	}
	if(msgid == NULL) {
		error_log(ERRLOG_REPORT, xover_phrases[13], linein, NULL);
	}	
	else {
		if(masterk->child.Pid != -1) {
		/* send to child program */
			match = (killprg_sendxover(master, linein) == TRUE) ? REASON_PRG : REASON_NONE;
		}
#ifdef PERL_EMBED
		else if(masterk->perl_int != NULL) {
			/* send to perl subroutine */			
			match = (killperl_sendxover(master, linein) == TRUE) ? REASON_PERL : REASON_NONE;
		}
#endif
		else {
			/* we have to check against master killfile and group kill file */
			match = chk_a_group( master, &(masterk->master), master->xoverview, &reason);
		
			if((grpskill != NULL) && (match == REASON_NONE || masterk->grp_override == TRUE)) {
				/* we have to check against group */
				while ( grpskill != NULL) {
					grpkill = grpskill->grp;
					match = chk_a_group( master, &(grpkill->match), master->xoverview, &reason);
					if(grpkill->delkeep == DELKEEP_KEEP) {
						if(match != REASON_NONE) {
							keep = TRUE;
						}
						else {
							del = TRUE;
							why = REASON_NOKEEP;
						}
					}
					else {
						if(match != REASON_NONE) {
							del = TRUE;
							why = match;
						}
						else {
							keep = TRUE;
						}
					}
					grpskill = grpskill->next;
				}
			
				/* now do tie-breaking */
				if(del == FALSE && keep == FALSE) {
				/* match nothing */
					match = REASON_NONE;
				}
				else if(del != keep) {
				/* either keep or del */
					match = ( del == TRUE) ? why : REASON_NONE;
				}
				else {
				/* do tie-breaking */
					match = (tie_delete == TRUE) ? REASON_TIE : REASON_NONE;
				}
			}
		}
		
		/* now we need to null terminate the msgid, for allocing or printing */

		if(msgid != NULL) {
			tmp = msgid[msgidlen];
			msgid[msgidlen] = '\0';
		}
	
		if(match == REASON_NONE) {
			/* we keep it */
			retval= allocnode(master, msgid, MANDATORY_OPTIONAL, group, msgnr);
		}
		else if(masterk->logyn != KILL_LOG_NONE) {
			/* only open this once */
			if(masterk->logfp == NULL) {
				if((masterk->logfp = fopen(full_path(FP_GET, FP_TMPDIR, master->kill_log_name), "a")) == NULL) {
					MyPerror(xover_phrases[11]);
				}
			}
			
			if(masterk->logfp != NULL) {
				/* Log it */
				if(match == REASON_HEADER) {
					print_phrases(masterk->logfp, xover_phrases[9], group, reason, msgid, NULL);
				}
				else {
					print_phrases(masterk->logfp, xover_phrases[9], group, xover_reasons[match], msgid, NULL);
				}
				/* restore the message-id end of string so entire line prints */
				if(msgid != NULL) {
					msgid[msgidlen] = tmp;
				}
				if(masterk->xover_log_long == TRUE) {
					/* print the xover formatted to look like a message header */
					overv = master->xoverview;
					while(overv != NULL ) {
						tmp = overv->field[overv->fieldlen]; /* null terminate it */
						overv->field[overv->fieldlen] = '\0';
						print_phrases(masterk->logfp, "%v1% %v2%\n", overv->header, overv->field);
						overv->field[overv->fieldlen] = tmp; /* restore it */
						overv = overv->next ;
					}
				}
				else if(masterk->logyn == KILL_LOG_LONG) {
					/* print the xover as well */
					print_phrases(masterk->logfp, xover_phrases[10],linein, NULL);
				}
			}
		}
	}
	
	return retval;
}
/*----------------------------------------------------------------------------------------*/
void get_xoverview(PMaster master) {
	/* get in the xoverview.fmt list, so we can parse what xover returns later */
	/* we'll put em in a linked list */
	
	int done, retval, len, full;
	char *resp;
	POverview tmp, tmp2, curptr;
	
	retval = RETVAL_OK;
	curptr = NULL;  /* where we are currently at in the linked list */
	
	if(send_command(master, "list overview.fmt\r\n", &resp, 215) == RETVAL_OK) {
		done = FALSE;
		/* now get em in, until we hit .\n which signifies end of response */
		while(done != TRUE) {
			sgetline(master->sockfd, &resp);
			if(master->debug == TRUE) {
				do_debug("Got line--%s", resp);
			}
			if(strcmp(resp, ".\n") == 0) {
				done = TRUE;
			}
			else if(retval == RETVAL_OK) {
				/* now save em if we haven't run out of memory */
				len = strlen(resp);
                                /* does this have a full flag on it, which means the field name */
                                /* will be in the xover string */
				full = (strstr(resp, ":full") == NULL) ?  FALSE : TRUE; 
				/* now get rid of everything back to : */
				while(resp[len] != ':') {
					resp[len--] = '\0';
				}
				len++; /* so we point past colon */
				
				if((tmp = malloc(sizeof(Overview))) == NULL) {
					error_log(ERRLOG_REPORT, xover_phrases[12], NULL);
					retval = RETVAL_ERROR;
				}
				else if((tmp->header = calloc(sizeof(char), len+1)) == NULL) {
					error_log(ERRLOG_REPORT, xover_phrases[12], NULL);
					retval = RETVAL_ERROR;
				}
				else {
					/* initialize the structure */

					/* do this first so we don't wipe out with null termination */ 
					strncpy(tmp->header, resp, len); /* so we get the colon */
					tmp->header[len] = '\0';
					tmp->next = NULL;
					tmp->field = NULL;
					tmp->fieldlen = 0;
					tmp->full = full;
		
					
					if(curptr == NULL) {
						/* at head of list */
						curptr = tmp;
						master->xoverview = tmp;
					}
					else {
						/* add to linked list */
						curptr->next = tmp;
						curptr = tmp;
					}
				}
			}
		}
	}
	if(retval != RETVAL_OK) {
		/* free up whatever alloced */
		tmp = master->xoverview;
		while (tmp != NULL) {
			tmp2 = tmp;
			if(tmp->header != NULL) {
				free(tmp->header);
			}
			free(tmp);
			tmp = tmp2;
		}
		master->xoverview = NULL;
	}
	if(master->debug == TRUE && (tmp = master->xoverview) != NULL) {
		do_debug("--Xoverview.fmt list\n");
		while(tmp != NULL) {
			if(tmp->header != NULL) {
				do_debug("item = %s -- full = %s\n", tmp->header, true_str(tmp->full));
			}
			tmp = tmp->next;
		}
		do_debug("--End Xoverview.fmt list\n");
	}
}
/*-------------------------------------------------------------------------------------*/
int chk_a_group(PMaster master, POneKill grp, POverview overv, char **reason) {
	/* return REASON_  if xover matches group */
	/* linein should be at tab after the Message Number */
 
	int i, match = REASON_NONE;
	unsigned long bytes;
	int lines;
	char tchar, *tptr;
	static char reasonstr[MAXLINLEN];
	
	pmy_regex ptr;
	POverview tmp;
	*reason = reasonstr;

	tmp = overv;
	/* go thru each header field, and see if we must test against it */
	while ( tmp != NULL && match == REASON_NONE) {
		/* only do the test if we have a valid header & field to test against */
		if(tmp->field != NULL && tmp->header != NULL) {
			/* test the size of the body of the article */
			if(grp->bodybig > 0 || grp->bodysmall > 0) {
				if(strcasecmp(tmp->header, "Bytes:") == 0) {
					sscanf(tmp->field, "%lu", &bytes); /* convert ascii to long */
					if((grp->bodybig > 0) && (bytes > grp->bodybig)) {
						match = REASON_HIBYTES;
					}
					else if((grp->bodysmall > 0) && (bytes < grp->bodysmall)) {
						match = REASON_LOWBYTES;
					}
				}	
			}
			/* test the number of lines in the article */
			if((match == REASON_NONE) && (grp->hilines > 0 || grp->lowlines > 0)) {
				if(strcasecmp(tmp->header, "Lines:") == 0) {
					sscanf(tmp->field, "%d", &lines);
					if((grp->hilines > 0) && (lines > grp->hilines)) {
						match = REASON_HILINES;
					}
					else if((grp->lowlines > 0) && (lines < grp->lowlines)) {
						match = REASON_LOWLINES;
					}
				}
			}
			/* check the number of xrefs */
			if(match == REASON_NONE && grp->maxxref > 0) {
				if(strcasecmp(tmp->header, "Xref:") == 0) {
					i = 0;
					tptr = tmp->field;
					while(i <= grp->maxxref && *tptr != '\0' ) {
						if(*tptr == COLON ) {
							i++;
						}
						tptr++;
					}
					if(i > grp->maxxref) {
						match = REASON_XREF;
					}
				}
			}
			/* match against any of the headers */
			if(match == REASON_NONE) {
				ptr = grp->list;
				while(match == REASON_NONE && ptr != NULL) {
					if(strcasecmp(tmp->header, ptr->header) == 0) {
						/* we need to null terminate the field and restore it later */
						tchar = tmp->field[tmp->fieldlen];
						tmp->field[tmp->fieldlen] = '\0';
						if(regex_block(tmp->field, ptr, master->debug) == TRUE) {
							match = REASON_HEADER;
							sprintf(reasonstr, "%s-%s%s", xover_reasons[REASON_HEADER], tmp->header, tmp->field);
						}
						tmp->field[tmp->fieldlen] = tchar;
					}
					ptr = ptr->next;
				}
			}
		}
		tmp = tmp->next;
	}
	return match;
}
/*---------------------------------------------------------------------------*/
int match_group(char *match_grp, char *group, int debug) {
	/* does match match group?  match may contain wildcards */
	int match = FALSE;
	if(match_grp != NULL && group != NULL) {
		if(debug == TRUE) {
			do_debug("Xover - matching %s against %s\n", match_grp, group);
		}
		
		while( *group == *match_grp && *group != '\0') {
			group++;
			match_grp++;
		}
		if(*match_grp == '\0' || *match_grp == '*') {
                        /* wildcard match or end of string, they match so far, so they match */
			match = TRUE;
		}
		if(debug == TRUE) {
			do_debug("match = %s\n", true_str(match));
		}
		
	}
	return match;
}
/*-------------------------------------------------------------------------------*/
int get_xover(PMaster master, char *group, long startnr, long endnr) {
	
	int len, nr, done, retval = RETVAL_OK;
	char cmd[MAXLINLEN], *resp, *ptr, *msgid;
	long msgnr;
	
	sprintf(cmd, "xover %ld-%ld\r\n", startnr,endnr);
	retval = send_command(master, cmd, &resp, 0);
	if(retval == RETVAL_OK) {
		number(resp, &nr);
		switch(nr) {
		  default:
		  case 412:
		  case 502:
			  /* abort on these errors */
			  error_log(ERRLOG_REPORT, xover_phrases[0], resp, NULL);
			  retval = RETVAL_NOXOVER;
			  break;
		  case 420:
			  /* no articles available, no prob */
			  break;
		  case 224:
			  /* got a list coming at us */
			  done = FALSE;
			  while((done == FALSE) && (retval == RETVAL_OK)) {
				  len = sgetline(master->sockfd, &resp);
				  if(master->debug == TRUE) {
					  do_debug("Got xover line: %s", resp);
				  }
				  
				  /* have we got the last one? */
				  if((len == 2) && (strcmp(resp, ".\n") == 0)) {
					  done = TRUE;
				  }
				  else if(len < 0) {
					  retval =RETVAL_ERROR;
				  }
				  else {
					  ptr  = get_long(resp, &msgnr);  /* get our message number */
					  msgid = find_msgid(master, ptr);        /* find the msg-id */
					  if(msgid == NULL) {
						  error_log(ERRLOG_REPORT, xover_phrases[13], resp);
					  }
					  else {						  
						  /* alloc the memory for it */
						  retval= allocnode(master, msgid, MANDATORY_OPTIONAL, group, msgnr);
					  }
					  
				  }
			  }
			  break;
		}
	}
	return retval;
}
/*-----------------------------------------------------------------------------------*/
/* find the msgid in a xover, and return it null-terminated, so can alloc memory, etc*/
/*-----------------------------------------------------------------------------------*/
char *find_msgid(PMaster master, char *xover) {

	char *ptr, *ptr2, *retval = NULL;
	POverview pov;

	pov = master->xoverview;
	ptr = xover;

	/* now go thru and find the Message-ID in the xoverview, using the overview.fmt */
	/* to tell us which field is which */

	if(ptr != NULL) {
		while ( *ptr != '\0' && pov != NULL && strcmp(pov->header, "Message-ID:") != 0) {
			/* go to the next field, past the tab */
			pov = pov->next;
			while(*ptr != '\t' && *ptr != '\0') {
				ptr++;
			}
			if(*ptr != '\0') {
				ptr++; /* past the tab */
			}
		}
		if(*ptr != '\0' && pov != NULL) {
			/* bingo, found it */
			/* find the start of the msgid */
			while(*ptr != '\0' && *ptr != '<') {
				ptr++;
			}
			if(ptr != '\0') {
				ptr2 = ptr;
				/* NULL terminate the msgid */
				while(*ptr2 != '\0' && *ptr2 != '>') {
					ptr2++;
				}
				/* ONLY if we find a valid start and end to the MessageId */
				/* do we set a valid return value */
				if(*ptr2 != '\0') {
					*(ptr2+1) = '\0';
					retval = ptr;
				}
			}
		}
	}

	return retval;
}

	
	

	
