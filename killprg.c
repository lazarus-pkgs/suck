#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_DIRENT_H
# include <dirent.h>
#else
# define dirent direct
# ifdef HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# ifdef HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
#endif

#ifdef HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif
#ifndef WEXITSTATUS
# define WEXITSTATUS(stat_val) ((unsigned)(stat_val) >>8)
#endif
#ifndef WIFEXITED
# define WIFEXITED(stat_val) (((stat_val) & 255) == 0)
#endif

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#include <string.h>
#include <sys/stat.h>
#include <sys/param.h>

#ifdef DMALLOC
#include <dmalloc.h>
#endif

#include "suck_config.h"
#include "both.h"
#include "suck.h"
#include "suckutils.h"
#include "killfile.h"
#include "phrases.h"

#ifdef MYSIGNAL
#include <signal.h>
#endif



#ifdef PERL_EMBED
#include <EXTERN.h>
#include <perl.h>
#ifdef OLD_PERL
#ifndef ERRSV
# define ERRSV (GvSV(errgv)) /* needed for perl 5.004 and earlier */
#endif
#ifndef PL_na
# define PL_na (na)
#endif
#endif /* OLD_PERL */
#endif

/* KILLPRG.C -------------------------------------------------------------*/
/* this file contains all subroutines pertaining to forking a killfile    */
/* program, calling the program for each message and checking the results.*/
/* It also must handle gracefully the child dieing, etc.		  */
/*------------------------------------------------------------------------*/

/* function prototypes */
int find_in_path(char *);

#ifdef MYSIGNAL
RETSIGTYPE pipe_sighandler(int);
#endif

int killprg_forkit(PKillStruct mkill, char *args, int debug, int which) {
/*------------------------------------------------------------------------ */
/* take the char string args, parse it to get the program to check to see  */
/* if it exists, then try to fork, execl, and set up pipe to it.  If       */
/* successful, return TRUE, else return FALSE.				   */
/*-------------------------------------------------------------------------*/
	int i, newstdin[2], newstdout[2], retval = FALSE;
	char *prg[KILLPRG_MAXARGS+1], *myargs = NULL, *ptr, *nptr;

	/* first parse args into array */
	i = strlen(args);
	/* do this so can muck around with string with messing up original */
	if((myargs = malloc(i+1)) == NULL) {
		error_log(ERRLOG_REPORT, killp_phrases[0], NULL);
	}
	else {
		strcpy(myargs, args);
		/* strip off nl */
		if(myargs[i-1] == '\n') {
			myargs[i-1] = '\0';
		}

		/* build array of pointers to string, by finding spaces and */
		/* replacing them with \0 */
		i = 0;
		ptr = myargs;
		do {
			prg[i++] = ptr;
			nptr = strchr(ptr, ' ');
			if(nptr != NULL) {
				*nptr = '\0';
				nptr++;
			}
			ptr = nptr;
		}
		while(ptr != NULL && i < KILLPRG_MAXARGS);
		prg[i] = NULL;		/* must do this for execv */
	}

	if(debug == TRUE) {
		do_debug("Prg = %s, %d args\n", prg[0], i);
	}
	if(prg[0] != NULL && find_in_path(prg[0]) == TRUE ) {
		/* okay it exists, fork, execl, etc */
		/* first set up our pipes */
		if(pipe(newstdin) == 0) {
			if(pipe(newstdout) == 0) {
				retval = TRUE;
			}
			else {
				close(newstdin[0]);
				close(newstdin[1]);
			}
		}
		if(retval == FALSE) {
			MyPerror(killp_phrases[1]);
		}
		else {
#ifdef MYSIGNAL
			signal(SIGPIPE, pipe_sighandler);	/* set up signal handler if pipe breaks */
			signal_block(MYSIGNAL_ADDPIPE);		/* set up sgetline() to block signal */
#endif
			mkill->child.Pid=fork();
			if(mkill->child.Pid == 0) {
				/* in child */
				close(newstdin[1]); 	/* will only be reading from this */
				close(newstdout[0]);	/* will only be writing to this */
				close(0);
				close(1);		/* just to be on safe side */
				dup2(newstdin[0], 0);	/* copy new read to stdin */
				dup2(newstdout[1], 1);	/* copy new write to stdout */
				execvp(prg[0], prg);
				exit(-1); /* so if exec fails, we don't get two sucks running */
			}
			else if( mkill->child.Pid == -1) {
			  	/* whoops */
				MyPerror(killp_phrases[3]);
				retval = FALSE;
				/* close down all the pipes */
				close(newstdin[0]);
				close(newstdin[1]);
				close(newstdout[0]);
				close(newstdout[1]);
			}
			else {
				/* parent */
				close(newstdin[0]);	/* will only be writing to new stdin */
				close(newstdout[1]);	/* will only be reading from new stdout */
				/* so subroutine can read/write to child */
				mkill->child.Stdin = newstdin[1];
                                mkill->child.Stdout = newstdout[0];
				if ( which == KILL_KILLFILE) {
					/* we don't do this if it's an xover file, that's done otherwhere */
					mkill->killfunc = chk_msg_kill_fork;	/* point to our subroutine */
				}
			}
		}

	}
	if(myargs != NULL) {
		free(myargs);
	}
	return retval;

}
/*-----------------------------------------------------------------------*/
int find_in_path(char *prg) {
	/* parse the path and search thru it for the program to see if it exists */

	int retval = FALSE, len;
	char fullpath[PATH_MAX+1], *ptr, *mypath, *nptr;
	struct stat buf;

	/* if prg has a slant bar in it, its an absolute/relative path, no search done */
	if(strchr(prg, '/') == NULL) {
		ptr = getenv("PATH");
		if(ptr != NULL) {
			len = strlen(ptr)+1;
			/* now have to copy the environment, since I can't touch the ptr */
			if((mypath = malloc(len)) == NULL) {
				error_log(ERRLOG_REPORT, "%s %s", killp_phrases[2], NULL);
			}
			else {
				strcpy(mypath, ptr);
				ptr = mypath;	/* start us off */
				do {
					nptr = strchr(ptr, PATH_SEPARATOR);
					if(nptr != NULL) {
						*nptr = '\0';		/* null terminate the current one */
						nptr++;			/* move past it */
					}
					/* build the fullpath name */
					strcpy(fullpath, ptr);
					if(ptr[strlen(ptr)-1] != '/') {
						strcat(fullpath, "/");
					}
					strcat(fullpath, prg);
					/* okay now have path, check to see if it exists */
					if(stat(fullpath, &buf) == 0) {
						if(S_ISREG(buf.st_mode)) {
							retval = TRUE;
						}
					}
					/* go onto next one */
					ptr = nptr;
				}
				while(ptr != NULL && retval == FALSE);
				free(mypath);
			}
		}
	}
	/* last ditch try, in case of a relative path or current directory */
	if (retval == FALSE) {
		if(stat(full_path(FP_GET_NOPOSTFIX, FP_NONE, prg), &buf) == 0) {
			if(S_ISREG(buf.st_mode)) {
				retval = TRUE;
			}
		}
	}
	if(retval == FALSE) {
		error_log(ERRLOG_REPORT, killp_phrases[3], prg, NULL);
	}
	return retval;
}
/*----------------------------------------------------------------------------*/
int chk_msg_kill_fork(PMaster master, PKillStruct pkill, char *header, int headerlen) {
	/* write the header to ChildStdin, read from ChildStdout */
	/* read 0 if get whole message, 1 if skip */
	/* return TRUE if kill article, FALSE if keep  */

	int retval = FALSE, status;
	char keepyn, keep[4], len[KILLPRG_LENGTHLEN+1];
	pid_t waitstatus;

	/* first make sure our child is alive WNOHANG so if its alive, immed return */
	keepyn = -1;	/* our error status */
	waitstatus = waitpid(pkill->child.Pid, &status, WNOHANG);
	if(waitstatus == 0) { /* child alive */
		if(master->debug == TRUE) {
			do_debug("Writing to child\n");
		}
		/* first write the length down */
		sprintf(len, "%-*d\n", KILLPRG_LENGTHLEN-1, headerlen);
		if(write(pkill->child.Stdin, len, KILLPRG_LENGTHLEN) <= 0) {
			error_log(ERRLOG_REPORT,  killp_phrases[4], NULL);
		}
		/* then write the actual header */
		else if(write(pkill->child.Stdin, header, headerlen) <= 0) {
			error_log(ERRLOG_REPORT,  killp_phrases[4], NULL);
		}
		/* read the result back */
		else {
			if(master->debug == TRUE) {
				do_debug("Reading from child\n");
			}
			if(read(pkill->child.Stdout, &keep, 2) <= 0) {
				error_log(ERRLOG_REPORT,  killp_phrases[5], NULL);
			}
			else {
				if(master->debug == TRUE) {
					keep[2] = '\0';	/* just in case */
					do_debug("killprg: read '%s'\n", keep);
				}
				keepyn = keep[0] - '0';	/* convert ascii to 0/1 */
			}
		}
	}
	else if(waitstatus == -1) { /* huh? */
		MyPerror( killp_phrases[6]);
	}
	else { /* child died */
		error_log(ERRLOG_REPORT,  killp_phrases[7], NULL);
	}

	switch (keepyn) {
	  case 0:
		retval = FALSE;
		break;
	  case 1:
		retval = TRUE;
		if(pkill->logyn != KILL_LOG_NONE) {
			/* log it */
			if(pkill->logfp == NULL) {
				if((pkill->logfp = fopen(full_path(FP_GET, FP_TMPDIR, master->kill_log_name), "a")) == NULL) {
					MyPerror(killp_phrases[12]);
				}
			}
			if(pkill->logfp != NULL) {
				/* first print our one line reason */
				print_phrases(pkill->logfp, killp_phrases[13], (master->curr)->msgnr, NULL);
				if(pkill->logyn == KILL_LOG_LONG) {
					/* print the header as well */
					print_phrases(pkill->logfp, "%v1%", header, NULL);
				}
			}
		}
		if(master->debug == TRUE) {
			do_debug("Kill program killed: %s", header);
		}
		break;
	  default: /* error in child don't use anymore */
		retval = FALSE;
		pkill->killfunc = chk_msg_kill; /* back to normal method */
	}

	return retval;
}
/*----------------------------------------------------------------------*/
void killprg_closeit(PKillStruct master) {
	/* tell it to close down by writing a length of zero */
	/* then wait for the pid to close down*/
	char len[KILLPRG_LENGTHLEN+1];

	sprintf(len, "%-*d\n", KILLPRG_LENGTHLEN-1, 0);

	write(master->child.Stdin, len, KILLPRG_LENGTHLEN);
	waitpid(master->child.Pid, NULL, 0);

}
/*---------------------------------------------------------------------*/
#ifdef MYSIGNAL
RETSIGTYPE pipe_sighandler(int what) {

	/* we don't have to do anything else, since the routine above will detect a dead child */
	/* and handle it appropriately*/

	int status;
	error_log(ERRLOG_REPORT, killp_phrases[8], NULL);
	wait(&status);
	if(WIFEXITED(status) != 0) {
		error_log(ERRLOG_REPORT, killp_phrases[9], str_int(WEXITSTATUS(status)), NULL);
	}
	else if(WIFSIGNALED(status) != 0) {
		error_log(ERRLOG_REPORT, killp_phrases[10], str_int(WTERMSIG(status)), NULL);
	}
	else {
		error_log(ERRLOG_REPORT, killp_phrases[11], NULL);
	}
}
#endif
#ifdef PERL_EMBED
/*---------------------------------------------------------------------*/
/* MUCH OF THIS CODE COMES From the PERLEMBED man pages that comes */
/* with perl */
/*---------------------------------------------------------------------*/
int killperl_setup(PKillStruct master, char *prg, int debug, int which) {
	int retval = TRUE;
	char *ptr = NULL;

	char *prgs[] = { ptr, ptr};


	if((master->perl_int = perl_alloc()) == NULL) {
		retval = FALSE;
		error_log(ERRLOG_REPORT, killp_phrases[14], NULL);
	}
	else {
		if(debug == TRUE) {
			do_debug("Perl program name = %s\n", prg);
		}

                perl_construct(master->perl_int);
                prgs[1] = prg;
                if(perl_parse(master->perl_int, NULL, 2, prgs, NULL) == 0) {
			perl_run(master->perl_int);
			if(which == KILL_KILLFILE) {
				/* we don't do this for xover, it's handled separately */
				master->killfunc = chk_msg_kill_perl;
			}
		}
		else {
			error_log(ERRLOG_REPORT, killp_phrases[15], prg, NULL);
			killperl_done(master);
		}

	}
        return retval;
}
/*--------------------------------------------------------------------------*/
void killperl_done(PKillStruct master) {
        perl_destruct(master->perl_int);
        perl_free(master->perl_int);
        master->perl_int = NULL;
	master->killfunc=chk_msg_kill; /* restore default function */
}
/*----------------------------------------------------------------------------*/
int chk_msg_kill_perl(PMaster master, PKillStruct killp, char *hdr, int hdrlen) {
	/* return TRUE to kill article, FALSE to keep */
	/* this code comes from the both perlembed and perlcall man pages */
	int i, retval = FALSE;
	char *args[2] = { NULL, NULL};

	dSP; /* Perl stack pointer */

	/* first set up args */
	args[0] = hdr;
	if(master->debug == TRUE) {
		do_debug("Calling %s\n", PERL_PACKAGE_SUB);
	}

	ENTER;
	SAVETMPS;
	PUSHMARK(SP);

	i = perl_call_argv(PERL_PACKAGE_SUB, G_SCALAR | G_EVAL | G_KEEPERR, args);
	SPAGAIN;

	if(SvTRUE(ERRSV)) {
                error_log(ERRLOG_REPORT, killp_phrases[16], SvPV(ERRSV,PL_na), NULL);
		POPs;
		killperl_done(killp);
	}
	else if(i != 1) {
		error_log(ERRLOG_REPORT, killp_phrases[17], PERL_PACKAGE_SUB, NULL);
		killperl_done(killp);
	}
	else {
		/* 0 = keep 1 = delete */
		i = POPi;
		if(master->debug == TRUE) {
			do_debug("retval = %d\n", i);
		}

		retval = (i == 0) ? FALSE : TRUE;
	}
	PUTBACK;
	FREETMPS;
	LEAVE;
        if(retval == TRUE && killp->logyn != KILL_LOG_NONE) {
		/* log it */
		if(killp->logfp == NULL) {
			if((killp->logfp = fopen(full_path(FP_GET, FP_TMPDIR, master->kill_log_name), "a")) == NULL) {
				MyPerror(killp_phrases[19]);
			}
		}
		if(killp->logfp != NULL) {
                        /* first print our one line reason */
			print_phrases(killp->logfp, killp_phrases[18], (master->curr)->msgnr, NULL);
			if(killp->logyn == KILL_LOG_LONG) {
				/* print the header as well */
				print_phrases(killp->logfp, "%v1%", hdr, NULL);
			}
		}
        }

	return retval;
}
#endif
/*---------------------------------------------------------------------------------*/
void killprg_sendoverview(PMaster master) {

	PKillStruct killp;
	POverview overp;
	char buf[MAXLINLEN], len[KILLPRG_LENGTHLEN+1];
	int count, status;
	pid_t waitstatus;
#ifdef PERL_EMBED
	char *args[2] = { NULL, NULL};
#endif

	killp = master->xoverp;
	overp = master->xoverview;

	/* send the overview.fmt to the child process, if it exists */
	if(killp != NULL && overp != NULL) {
		/* first, rebuild the overview.fmt */
		count = 0;
		buf[0] = '\0';
		while(overp != NULL) {
			if(count > 0) {
				strcat(buf, "\t"); /* add the separator */
			}
			strcat(buf, overp->header);
			if(overp->full == TRUE) {
				strcat(buf, "full");
			}
			overp = overp->next;
			count++;
		}
		strcat(buf, "\n");
		count = strlen(buf);
		if(killp->child.Pid != -1) {
			waitstatus = waitpid(killp->child.Pid, &status, WNOHANG);
			if(waitstatus == 0) {
				if(master->debug == TRUE) {
					do_debug("sending overview.fmt, len = %d - %s", count, buf);
				}
						/* first write the length down */
				sprintf(len, "%-*d\n", KILLPRG_LENGTHLEN-1, count);
				if(write(killp->child.Stdin, len, KILLPRG_LENGTHLEN) <= 0) {
					error_log(ERRLOG_REPORT,  killp_phrases[4], NULL);
				}
				/* then write the actual header */
				else if(write(killp->child.Stdin, buf, count) <= 0) {
					error_log(ERRLOG_REPORT,  killp_phrases[4], NULL);
				}
			}
		}
#ifdef PERL_EMBED
		/* this code comes from the both perlembed and perlcall man pages */

		else if(killp->perl_int != NULL) {
			dSP; /* Perl stack pointer */

			/* first set up args */
			args[0] = buf;
			if(master->debug == TRUE) {
				do_debug("Calling %s\n", PERL_OVER_SUB);
			}

			ENTER;
			SAVETMPS;
			PUSHMARK(SP);

			status = perl_call_argv(PERL_OVER_SUB, G_VOID | G_EVAL | G_KEEPERR, args);
			SPAGAIN;

			if(SvTRUE(ERRSV)) {
				error_log(ERRLOG_REPORT, killp_phrases[16], SvPV(ERRSV,PL_na), NULL);
				POPs;
				killperl_done(killp);
			}
			else if(status != 1) {
				if(master->debug == TRUE) {
					do_debug("return from perl_call_argv = %d\n", status);
				}

				error_log(ERRLOG_REPORT, killp_phrases[17], PERL_OVER_SUB, NULL);
				killperl_done(killp);
			}
			PUTBACK;
			FREETMPS;
			LEAVE;
		}
#endif
	}
}
/*----------------------------------------------------------------------------------*/
int killprg_sendxover(PMaster master, char *linein) {

	/* this is almost a carbon copy of chk_msg_kill_fork() */
	int retval = FALSE, status, headerlen;
	char keepyn, keep[4], len[KILLPRG_LENGTHLEN+1];
	pid_t waitstatus;
	PKillStruct pkill;

	headerlen = strlen(linein);
	pkill = master->xoverp;

	/* first make sure our child is alive WNOHANG so if its alive, immed return */
	keepyn = -1;	/* our error status */
	waitstatus = waitpid(pkill->child.Pid, &status, WNOHANG);
	if(waitstatus == 0) { /* child alive */
		if(master->debug == TRUE) {
			do_debug("Writing to child\n");
		}
		/* first write the length down */
		sprintf(len, "%-*d\n", KILLPRG_LENGTHLEN-1, headerlen);
		if(write(pkill->child.Stdin, len, KILLPRG_LENGTHLEN) <= 0) {
			error_log(ERRLOG_REPORT,  killp_phrases[4], NULL);
		}
		/* then write the actual header */
		else if(write(pkill->child.Stdin, linein, headerlen) <= 0) {
			error_log(ERRLOG_REPORT,  killp_phrases[4], NULL);
		}
		/* read the result back */
		else {
			if(master->debug == TRUE) {
				do_debug("Reading from child\n");
			}
			if(read(pkill->child.Stdout, &keep, 2) <= 0) {
				error_log(ERRLOG_REPORT,  killp_phrases[5], NULL);
			}
			else {
				if(master->debug == TRUE) {
					keep[2] = '\0';	/* just in case */
					do_debug("killprg: read '%s'\n", keep);
				}
				keepyn = keep[0] - '0';	/* convert ascii to 0/1 */
			}
		}
	}
	else if(waitstatus == -1) { /* huh? */
		MyPerror( killp_phrases[6]);
	}
	else { /* child died */
		error_log(ERRLOG_REPORT,  killp_phrases[7], NULL);
	}

	retval = ( keepyn == 1) ? TRUE : FALSE;

	return retval;
}
/*-----------------------------------------------------------------------------------*/
#ifdef PERL_EMBED
int killperl_sendxover(PMaster master, char *linein) {

	/* this routine is almost a carbon copy  of chk_msg_kill_perl() */

	int i, retval = FALSE;
	char *args[2] = { NULL, NULL};

	dSP; /* Perl stack pointer */

	/* first set up args */
	args[0] = linein;
	if(master->debug == TRUE) {
		do_debug("Calling %s\n", PERL_XOVER_SUB);
	}

	ENTER;
	SAVETMPS;
	PUSHMARK(SP);

	i = perl_call_argv(PERL_XOVER_SUB, G_SCALAR | G_EVAL | G_KEEPERR, args);
	SPAGAIN;

	if(SvTRUE(ERRSV)) {
                error_log(ERRLOG_REPORT, killp_phrases[16], SvPV(ERRSV,PL_na), NULL);
		POPs;
		killperl_done(master->xoverp);
	}
	else if(i != 1) {
		error_log(ERRLOG_REPORT, killp_phrases[17], PERL_XOVER_SUB, NULL);
		killperl_done(master->xoverp);
	}
	else {
		/* 0 = keep 1 = delete */
		i = POPi;
		if(master->debug == TRUE) {
			do_debug("retval = %d\n", i);
		}

		retval = (i == 0) ? FALSE : TRUE;
	}
	PUTBACK;
	FREETMPS;
	LEAVE;
	return retval;
}
#endif
