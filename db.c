#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#ifndef O_SYNC
#define O_SYNC O_FSYNC
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#include <sys/types.h>
#endif

#ifdef DMALLOC
#include <dmalloc.h>
#endif

#include "suck_config.h"

#include "suck.h"
#include "suckutils.h"
#include "both.h"
#include "phrases.h"
#include "db.h"

/* ------------------------------------------------------------------------------ */
/* this file handles all the db routines, creating, deleting, adding records to,  */
/* flagging as downloaded, etc.  It replaces the old suck.restart and suck.sorted */
/* ------------------------------------------------------------------------------ */
int db_delete(PMaster master) {

	int retval = RETVAL_OK;
	const char *ptr;
	
	db_close(master); /* just in case */
	ptr = full_path(FP_GET, FP_TMPDIR, N_DBFILE);
	if(master->debug == TRUE) {
		do_debug("Unlinking %s\n", ptr);
	}
	if(unlink(ptr) != 0 && errno != ENOENT) {
		MyPerror(ptr);
		retval = RETVAL_ERROR;
	}

	return retval;
}
/*------------------------------------------------------------------------------*/
int db_write(PMaster master) {

	/* write out the entire database */
	int retval = RETVAL_OK;
	const char *fname;
	int fd;
	long itemnr, grpnr;
	PList itemon;
	PGroups grps;
	
	fname = full_path(FP_GET, FP_TMPDIR, N_DBFILE);
	if(master->debug == TRUE) {
		do_debug("Writing entire db - %s\n", fname);
	}
	
	if((fd = open(fname, O_WRONLY | O_CREAT | O_TRUNC
#ifdef EMX /* OS-2 */
		      | O_BINARY
#endif
		      , S_IRUSR | S_IWUSR)) == -1) {
		retval = RETVAL_ERROR;
		MyPerror(fname);
	}
	else {
		/* start at the head of the list, add our dbnr */
		itemon = master->head;

		/* first write out number of items, so can read em in later*/
		itemnr = master->nritems;
		write(fd, &itemnr, sizeof(itemnr));
		
		/* now write the list */
		itemon = master->head;
		itemnr = 0L;
		while ( itemon != NULL && retval == RETVAL_OK) {
			itemon->dbnr = itemnr;
			if(write(fd, itemon, sizeof(List)) != sizeof(List)) {
				error_log(ERRLOG_REPORT, suck_phrases[23], NULL);
				retval = RETVAL_ERROR;
			}
			itemon = itemon->next;
			itemnr++;
		}
		
		/* now write out the groups */
		/* first write out the number of items, so can read em in later */
		grps = master->groups;
		grpnr = 0;
		while(grps != NULL) {
			grps = grps->next;
			grpnr++;
		}
		write(fd, &grpnr, sizeof(grpnr));
		
		grps = master->groups;
		grpnr = 0L;
		while (grps != NULL) {
			if(write(fd, grps, sizeof(Groups)) != sizeof(Groups)) {
				error_log(ERRLOG_REPORT, suck_phrases[23], NULL);
				retval = RETVAL_ERROR;
			}
			grps = grps->next;
			grpnr++;
		}
		close(fd);
	}

	return retval;
}
/*--------------------------------------------------------------------------------*/
int db_read(PMaster master) {

	/* read in the entire database, and add it to the end of the current list */
	int retval = RETVAL_OK;
	const char *fname;
	int fd;
	PList itemon, ptr;
	PGroups grpon, gptr;
	long itemnr;
	
	fname = full_path(FP_GET, FP_TMPDIR, N_DBFILE);
	if(master->debug == TRUE) {
		do_debug("Reading entire db - %s\n", fname);
	}
	if((fd = open(fname, O_RDONLY
#ifdef EMX /* OS-2 */
		       | O_BINARY
#endif
		)) != -1) { /* read em in */
		print_phrases(master->msgs, suck_phrases[2], NULL);
		read(fd, &itemnr, sizeof(itemon)); /* get the number of items */
		
		master->nritems = itemnr;
		itemon = NULL;
		/* have to malloc em first */
		do { 
			if((ptr = malloc(sizeof(List))) == NULL) {
				retval = RETVAL_ERROR;
				error_log(ERRLOG_REPORT, suck_phrases[22], NULL);
			}
			else {
				if(read(fd, ptr, sizeof(List)) == sizeof(List)) {
					itemnr--;
					/* add to list */
					if( master->head == NULL) {
						master->head = ptr;
					}
					else {
						itemon->next = ptr;
					}
					itemon = ptr; /* so we can track where we're at */
					if(master->debug == TRUE) {
						do_debug("restart-read %s-%d-%ld-%d-%d-%d\n", ptr->msgnr,
							 ptr->groupnr, ptr->nr, ptr->mandatory, ptr->downloaded,
							 ptr->delete, ptr->sentcmd);
					}
				}
				else {
					retval = RETVAL_ERROR; /* didn't get in what we expected */
					MyPerror(fname);
				}
			}
			
		}
		while(retval == RETVAL_OK && itemnr > 0);
		/* now get the groups */
		/* get the count first */
		read(fd, &itemnr, sizeof(itemnr));
		grpon = NULL;
		do {
			if((gptr = malloc(sizeof(Groups))) == NULL) {
				retval = RETVAL_ERROR;
				error_log(ERRLOG_REPORT, suck_phrases[22], NULL);
			}
			else 
			{
				if(read(fd, gptr, sizeof(Groups)) == sizeof(Groups)) {
					itemnr--;
					if( master->groups == NULL) {
						master->groups = gptr;
					}
					else {
						grpon->next = gptr;
					}
					grpon = gptr;
				}
				else {
					retval = RETVAL_ERROR;
					MyPerror(fname);
				}
			}
		}
		while(retval == RETVAL_OK && itemnr > 0);
		close(fd);
		if(retval != RETVAL_OK) {
			/* free up what we brought in */
			itemon = master->head;
			master->nritems = 0;
			while (itemon != NULL) {
				ptr = itemon->next;
				free(itemon);
				itemon = ptr;
			}
			grpon = master->groups;
			while(grpon != NULL) {
				gptr = grpon->next;
				free(grpon);
				grpon = gptr;
			}
			master->head = NULL;
			master->groups = NULL;
		}
	}
	return retval;
}
/*-----------------------------------------------------------------------------*/
int db_open(PMaster master) {

	int retval = RETVAL_OK;
	const char *fname;
	
	if(master->db != -1) {
		/* just to be on the safe side */
		close(master->db);
	}
	fname = full_path(FP_GET, FP_TMPDIR, N_DBFILE);
	/* we have the sync on it, so that we force the write to disk */
	if((master->db = open (fname, O_WRONLY | O_SYNC
#ifdef EMX
			       | O_BINARY
#endif
		)) == -1) {
		MyPerror(fname);
		retval = RETVAL_ERROR;
	}
	return retval;
}
/*-----------------------------------------------------------------------------*/
void db_close(PMaster master) {
	if(master->db != -1) {
		close(master->db);
	}	
}
/*------------------------------------------------------------------------------*/
int db_mark_dled(PMaster master, PList itemon) {
	int retval = RETVAL_OK;
	off_t offset;
	
        /* first figure out where we need to be writing to */
	offset = sizeof(long) + (itemon->dbnr * sizeof(List));
        /* the extra long to skip the number we write out at beginning of file */
	if(lseek(master->db, offset, SEEK_SET) == (off_t)-1) {
		MyPerror(full_path(FP_GET, FP_TMPDIR, N_DBFILE));
		retval = RETVAL_ERROR;
	}
	else {
		itemon->downloaded = TRUE;
		if(write(master->db, itemon, sizeof(List)) != sizeof(List)) {
			MyPerror(full_path(FP_GET, FP_TMPDIR, N_DBFILE));
			error_log(ERRLOG_REPORT, suck_phrases[23], NULL);
			retval = RETVAL_ERROR;
		}
	}
	
	return retval;
}

