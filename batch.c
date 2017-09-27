#include <config.h>

#include <stdio.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

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

#ifdef DMALLOC
#include <dmalloc.h>
#endif

#include "suck_config.h"
#include "suck.h"
#include "both.h"
#include "batch.h"
#include "suckutils.h"
#include "phrases.h"
#include "active.h"
#include "timer.h"

/*--------------------------------------------------------*/
/* function prototypes */
int post_one_msg(PMaster, int, char *);

/*----------------------------------------------------------*/
int do_innbatch(PMaster master) {
	/* build batch file that contains article listing */
	/* needed by innxmit */
	/* this is done by searching thru MSGDIR to find files */
	/* which match our naming convention */

	int i, retval = RETVAL_OK;
	FILE *fptr;
	const char *tmp;
	DIR *dptr;
	struct dirent *entry;

	print_phrases(master->msgs, batch_phrases[3], NULL);
	
	if((fptr = fopen(master->batchfile, "w")) == NULL) {
		MyPerror(master->batchfile);
		retval = RETVAL_ERROR;
	}
	else if((dptr = opendir(full_path(FP_GET, FP_MSGDIR, ""))) == NULL) {
		MyPerror(full_path(FP_GET, FP_MSGDIR, ""));
		retval = RETVAL_ERROR;
		fclose(fptr);
	}
	else {
		tmp = full_path(FP_GET_POSTFIX, 0, "");		/* this will be string we search for */
		/* look for entries which have our postfix */
		while((entry = readdir(dptr)) != NULL && retval == RETVAL_OK) {
			/* ignore hidden files */
			if(entry->d_name[0] != '.' && strstr(entry->d_name, tmp) != NULL) {
				i = fprintf(fptr, "%s\n", full_path(FP_GET_NOPOSTFIX, FP_MSGDIR, entry->d_name));
				if(i <= 0) {
					retval = RETVAL_ERROR;
					MyPerror(master->batchfile);
				}
			}
		}
		fclose(fptr);
		closedir(dptr);
	}
	return retval;
}
/*----------------------------------------------------------*/
int do_rnewsbatch(PMaster master) {

	/* build rnews formated file of articles */
	/* this is done by searching thru MSGDIR to find files */
	/* which match our naming convention */

	/* if max_file_size > 0, then create multiple files up to  max file size */

	int i, x, batchnr = 0, retval = RETVAL_OK;
	FILE *fptr = NULL, *fpin;
	const char *tptr, *tmp;
	char buf[MAXLINLEN];
	DIR *dptr;
	struct dirent *entry;
	struct stat sbuf, tbuf;
	long cursize = 0L;

	print_phrases(master->msgs, batch_phrases[4], NULL);

	if((dptr = opendir(full_path(FP_GET, FP_MSGDIR, ""))) == NULL) {
		MyPerror(full_path(FP_GET, FP_MSGDIR, ""));
		retval = RETVAL_ERROR;
	}
	else {
		tmp = full_path(FP_GET_POSTFIX, 0, "");	/* this will be string we search for */
		/* look for entries which have our postfix */
		while(retval == RETVAL_OK && (entry = readdir(dptr))) {
			/* ignore hidden files */
			if(entry->d_name[0] != '.' && strstr(entry->d_name, tmp) != NULL) {
				tptr = full_path(FP_GET_NOPOSTFIX, FP_MSGDIR, entry->d_name);
				if(stat(tptr, &sbuf) != 0 || (fpin = fopen(tptr, "r")) == NULL) {
					MyPerror(tptr);
					retval = RETVAL_ERROR;
				}
				else {
					if( cursize == 0 ) {
						if(fptr != NULL) {
							/* close old file */
							fclose(fptr);
							batchnr++;
						}
						/* have to open file */
						if(batchnr == 0) {
							strcpy(buf, master->batchfile);
						}
						else {
							sprintf(buf, "%s%d", master->batchfile, batchnr);
						}
						if(master->debug == TRUE) {
							do_debug("BATCH FILE: %s\n", buf);
						}
						if(stat(buf, &tbuf) == 0) {
							/* whoops file already exists */
							MyPerror(buf);
							retval = RETVAL_ERROR;
						}
						else if((fptr = fopen(buf, "w")) == NULL) {
							MyPerror(buf);
							retval = RETVAL_ERROR;
						}
					}
					if(retval == RETVAL_OK) {
						/* first put #! rnews size */
						fprintf(fptr, "#! rnews %ld\n", (long) sbuf.st_size);
						/* use fread/fwrite in case lines are longer than MAXLINLEN */
						while((i = fread(buf, 1, MAXLINLEN, fpin)) > 0 && retval == RETVAL_OK) {
							x = fwrite(buf, 1, i, fptr);
							if(x != i) {
								/* error writing file */
								retval = RETVAL_ERROR;
								MyPerror(buf);
							}
						}
						if(retval == RETVAL_OK ) {
							unlink(tptr);	/* we are done with it, nuke it */	
							cursize += sbuf.st_size;
							/* keep track of current file size, we can ignore the #! rnews */
							/* size, since it adds so little to the overall size */
							
							if(master->rnews_size > 0L && cursize > master->rnews_size) {
								/* reached file size length */
								cursize = 0L;	/* this will force a close and open on next article */
							}
						}
					}
					fclose(fpin);
				}
			}
		}
		if(fptr != NULL) {
			fclose(fptr);
		}
		
		closedir(dptr);
	}
	return retval;
}
/*----------------------------------------------------------------------------------*/
void do_lmovebatch(PMaster master) {
	/* fork lmove */
	const char *args[LMOVE_MAX_ARGS];
	int i, x;
	pid_t pid;

	print_phrases(master->msgs, batch_phrases[0], NULL);

	/* first build command */
	args[0] = "lmove";
	args[1] = "-c";
	args[2] = master->batchfile;
	args[3] = "-d";
	args[4] = full_path(FP_GET, FP_MSGDIR, "");
	i = 5;
	if(master->errlog != NULL) {
		args[i++ ] = "-E";
		args[i++ ] = master->errlog;
	}
	if(master->phrases != NULL) {
		args[i++] = "-l";
		args[i++] = master->phrases;
	}

	if(master->debug == TRUE) {
		args[i++] = "-D";
	}
	args[i] = NULL;

	if(master->debug == TRUE) { 
		do_debug("Calling lmove with args:");
		for(x = 0; x < i; x++) {
			do_debug(" %s", args[x]);
		}
		do_debug("\n");
	}

	/* now fork and execl */
	pid = fork();
	if(pid == 0) {
		/* in child */
		execvp(args[0], (char *const *) args);
		MyPerror(batch_phrases[2]);	/* only get here on errors */
		exit(-1); /* so we aren't running two sucks */
	}
	else if(pid < 0) {
		/* whoops */
		MyPerror(batch_phrases[1]);
	}
	else {
		/* in parent let the child finish */
		wait(NULL);
	}
}
/*---------------------------------------------------------------------------*/
int do_localpost(PMaster master) {
	
	/* post articles to local server using IHAVE */
	int sockfd, count = 0, retval = RETVAL_OK;
	FILE *fp_list;
	char msg[MAXLINLEN+1];
	const char *fname;

	TimerFunc(TIMER_START, 0, NULL);
	
	print_phrases(master->msgs, batch_phrases[5], master->localhost, NULL);
	
	if(master->batchfile == NULL) {
		error_log(ERRLOG_REPORT, batch_phrases[6], NULL);
		retval = RETVAL_ERROR;
	}
	else {
		fname = full_path(FP_GET, FP_TMPDIR, master->batchfile);
		if((fp_list = fopen(fname, "r")) == NULL ) {
			MyPerror(fname);
			retval = RETVAL_ERROR;
		}		 
		else {
			if((sockfd = connect_local(master)) < 0) {
				retval = RETVAL_ERROR;
			}
			else {
				while(retval == RETVAL_OK && fgets(msg, MAXLINLEN, fp_list) != NULL) {
					retval = post_one_msg(master, sockfd, msg);
					if(retval == RETVAL_OK) {
						count++;
					}
				}
				disconnect_from_nntphost(sockfd, master->local_ssl, &master->local_ssl_struct);
			}
			fclose(fp_list);
		}
		if(retval == RETVAL_OK) {
			if(master->debug == TRUE) {
				do_debug("deleting %s\n", fname);
			}
			unlink(fname);
		}
	}

	print_phrases(master->msgs, batch_phrases[10], str_int(count), NULL);
	TimerFunc(TIMER_TIMEONLY, 0,master->msgs);

	return retval;
	
}
/*------------------------------------------------------------------------------------------------*/
int post_one_msg(PMaster master, int sockfd, char *msg) {

	int len, nr, longline, do_unlink = FALSE, retval = RETVAL_OK;
	char *msgid, *resp, linein[MAXLINLEN+4]; /* the extra in case of . in first pos */
	FILE *fpi;
	
	/* msg contains the path and msgid */
	msgid = strstr(msg, " <"); /* find the start of the msgid */
	if(msgid == NULL) {
		error_log(ERRLOG_REPORT, batch_phrases[7], msg, NULL);
	}
	else {
		*msgid = '\0';	/* end the path name */
		msgid++;	/* so we point to the < */
		
		len = strlen(msgid);
		/* strip a nl */
		if(msgid[len-1] == '\n') {
			msgid[len-1] = '\0';
		}
		if(master->debug == TRUE) {
			do_debug("File Name = \"%s\"\n", msg);
		}
		
		if((fpi = fopen(msg, "r")) == NULL) {
			MyPerror(msg);
		}
		else {
			sprintf(linein, "IHAVE %s\r\n", msgid);
			if(master->debug == TRUE) {
				do_debug("sending command %s", linein);
			}
			sputline(sockfd, linein, master->local_ssl, master->local_ssl_struct);
			if(sgetline(sockfd, &resp, master->local_ssl, master->local_ssl_struct) < 0) {
				retval = RETVAL_ERROR;
			}
			else {
				if(master->debug == TRUE) {
					do_debug("got answer: %s", resp);
				}
				number(resp, &nr);
				/* added for prob */
				if(master->debug == TRUE) {
					do_debug("Answer=%d\n", nr);
				}
				if(nr == 435) {
					error_log(ERRLOG_REPORT, batch_phrases[11], msgid, resp, NULL);
					do_unlink = TRUE;
				}
				else if(nr != 335) {
					error_log(ERRLOG_REPORT, batch_phrases[8], msgid, resp, NULL);
				}
				else {
                                        /* send the article */
					longline = FALSE;
					while(fgets(linein, MAXLINLEN, fpi) != NULL) {
						/* added for prob */
						if(master->debug == TRUE) {
							do_debug("sending line-%s--\n", linein);
						}
						
						len = strlen(linein);
						if(longline == FALSE && linein[0] == '.') {
							/* double the . at beginning of line */
							memmove(linein+1,linein,++len);
							linein[0] = '.';
						}
						
						longline = ( linein[len - 1] == '\n' ) ? FALSE : TRUE ;
						if(longline == FALSE) {
							/* replace nl with cr nl */
							strcpy(&linein[len-1], "\r\n");
						}
						sputline(sockfd, linein, master->local_ssl, master->local_ssl_struct);
					}
					if(longline == TRUE) {
						/* end the last line */
						sputline(sockfd, "\r\n", master->local_ssl, master->local_ssl_struct);
					}
					/* end the article */
					sputline(sockfd, ".\r\n", master->local_ssl, master->local_ssl_struct);
					/* put in for prob */
					if(master->debug == TRUE) {
						do_debug("Finished sending article\n");
					}
					if(sgetline(sockfd, &resp, master->local_ssl, master->local_ssl_struct) < 0 ) {
						retval = RETVAL_ERROR;
					}
					else {
						if(master->debug == TRUE) {
							do_debug("Got response: %s", resp);
						}
						number(resp, &nr);
						if(nr == 437) {
							error_log(ERRLOG_REPORT, batch_phrases[12], msgid, resp, NULL);
							do_unlink = TRUE;
						}
						else if(nr == 235) {
							/* successfully posted, nuke it */
							do_unlink = TRUE;
						}
						else {
							error_log(ERRLOG_REPORT, batch_phrases[9], msgid, resp, NULL);
						}
					}			
				}
			}
			fclose(fpi);
			if(do_unlink == TRUE) {
				unlink(msg);
			}
			
		}
	}
		
	return retval;
	
}
/*--------------------------------------------------------------------------*/
void do_post_filter(PMaster master) {
	/* call lpost filter prgm with batchfile as argument */
	const char *msgdir;
	pid_t pid;
	
	if(master->post_filter != NULL) {
		msgdir = full_path(FP_GET, FP_MSGDIR, "");
		if(master->debug == TRUE) {
			do_debug("Running %s with %s as args\n", master->post_filter, msgdir);
		}
	
		pid = fork();
		if(pid == 0) {
			/* in child */
			if(execlp(master->post_filter, master->post_filter, msgdir, NULL) == -1) {
				/* big error */
				MyPerror(master->post_filter);
				exit(-1); /* so we aren't running two sucks */
			}		
		}
		else if(pid == -1) {
			/* whoops */
			MyPerror(master->post_filter);
		}
		else {
			/* parent, wait on child */
			wait(NULL);
		}
	}
}
