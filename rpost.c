#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <netdb.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#ifdef PERL_EMBED
#include <EXTERN.h>
#include <perl.h>
#ifdef OLD_PERL
#ifndef ERRSV
# define ERRSV (GvSV(errgv))  /* needed for perl 5.004 and earlier */
#endif
#ifndef PL_na
# define PL_na (na)
#endif
#endif /* OLD_PERL */u
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

#include "suck_config.h"
#include "both.h"
#include "phrases.h"

struct nntp_auth {
	char *userid;
	char *passwd;
	int autoauth;
};

typedef struct {
	char *batch;
	char *prefix;
	char *host;
	char *filter_args[RPOST_MAXARGS];
	int filter_argc;
	int filter_infilenr; /* nr of arg that we replace with infile */
	char *filter_outfile; /* pointer to name of outfile */
	int deleteyn;
	int do_modereader;
	int debug;
	struct nntp_auth auth;
	FILE *status_fptr;
	unsigned short int portnr;
	const char *phrases;
	char *rnews_file;
	char *rnews_path;
	int sockfd;
	int show_name;
	int ignore_readonly;
	int do_ssl;
	void *ssl_struct;
#ifdef PERL_EMBED
	PerlInterpreter *perl_int;
#endif
} Args, *Pargs;

/* function declarations */
int do_article(Pargs, FILE *);
int do_batch(Pargs);
int do_rnews(Pargs);
char *do_filter(Pargs, char *);
int send_command(Pargs, const char *, char **, int);
int do_authenticate(Pargs);
int scan_args(Pargs, int, char *[]);
int filter_post_article(Pargs, char *);
void log_fail(char *, char *);
void load_phrases(Pargs);
void free_phrases(void);
int parse_filter_args(Pargs myargs, int, char **);
#ifdef PERL_EMBED
void parse_perl(Pargs, char *);
void perl_done(Pargs);
char *filter_perl(Pargs, char *);
#endif


#define RNEWS_START  "#! rnews"   /* string that denotes the begin of a new message in rnews file */
#define RNEWS_START_LEN 8
#define TEMP_ARTICLE "tmp-article" /* name for temp article in rnews_path */

/* stuff needed for language phrases */
	/* set up defaults */	
char **rpost_phrases = default_rpost_phrases;
char **both_phrases = default_both_phrases;

enum { RETVAL_ERROR = -1, RETVAL_OK, RETVAL_ARTICLE_PROB, RETVAL_NOAUTH, RETVAL_UNEXPECTEDANS};
/*------------------------------------------------*/
int main(int argc, char *argv[], char *env[]) {
	char *inbuf;
	int response, retval, loop, fargc, i;
	struct hostent *hi;
	struct stat sbuf;
	char **args, **fargs;
	Args myargs;

	/* initialize everything */
	retval = RETVAL_OK;
	fargc = 0;
	fargs = NULL;

	myargs.batch = NULL;
	myargs.prefix =NULL;
	myargs.status_fptr = stdout;
	myargs.deleteyn = FALSE;
	myargs.auth.userid = NULL;
	myargs.auth.passwd = NULL;
	myargs.auth.autoauth = FALSE;
	myargs.do_modereader = FALSE;
	myargs.portnr = DEFAULT_NNRP_PORT;
	myargs.host = getenv("NNTPSERVER");		/* the default */
	myargs.phrases = NULL;
	myargs.debug = FALSE;
	myargs.rnews_file = NULL;
	myargs.rnews_path = NULL;
	myargs.filter_argc = 0;
	myargs.filter_infilenr = -1;
	myargs.filter_outfile = NULL;
	myargs.show_name = FALSE;
	myargs.ignore_readonly = FALSE;
	myargs.do_ssl = FALSE;
	myargs.ssl_struct = NULL;
#ifdef PERL_EMBED
	myargs.perl_int = NULL;
#endif

	/* have to do this next so if set on cmd line, overrides this */
#ifdef N_PHRASES		/* in case someone nukes def */
	if(stat(N_PHRASES, &sbuf) == 0 && S_ISREG(sbuf.st_mode)) {
		/* we have a regular phrases file make it the default */
		myargs.phrases = N_PHRASES;
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
				retval = scan_args(&myargs, fargc, fargs);
			}
		}   
		else {
			myargs.host = argv[1];
		}
		break;
	  default:
		for(loop=1;loop<argc && fargs == NULL;loop++) {
			if(argv[loop][0] == FILE_CHAR) {
				if((fargs = build_args(&argv[loop][1], &fargc)) != NULL) {
					retval = scan_args(&myargs, fargc, fargs);
				}
			}   
		}
		/* this is here so anything at command line overrides file */
		if(argv[1][0] != '-' && argv[1][0] != FILE_CHAR) {
			myargs.host = 	argv[1];
			argc-= 2;
			args = &(argv[2]);
		}
		else {
			args = &(argv[1]);
			argc--;
		}
		retval = scan_args(&myargs, argc, args);	
		break;
	}
	load_phrases(&myargs);	/* this has to be here so rest prints okay */

	if(retval == RETVAL_ERROR) {
		error_log(ERRLOG_REPORT, rpost_phrases[0], argv[0], NULL);
		error_log(ERRLOG_REPORT, rpost_phrases[1], NULL);
		error_log(ERRLOG_REPORT, rpost_phrases[2], NULL);
	}
	else {
		if(myargs.debug == TRUE) {
			do_debug("Rpost version %s\n",SUCK_VERSION);
			do_debug("myargs.batch = %s\n", null_str(myargs.batch));
			do_debug("myargs.prefix = %s\n", null_str(myargs.prefix));
			do_debug("myargs.filter_argc = %d\n", myargs.filter_argc);
			for(loop=0;loop<myargs.filter_argc;loop++) {
				do_debug("myargs.filter_args[%d] = %s\n", loop, null_str(myargs.filter_args[loop]));
			}
			do_debug("myargs.filter_infilenr = %d\n", myargs.filter_infilenr);
			do_debug("myargs.filter_outfile = %s\n", null_str(myargs.filter_outfile));
			do_debug("myargs.status_fptr = %s\n", (myargs.status_fptr == stdout) ? "stdout" : "not stdout");
			do_debug("myargs.deleteyn = %s\n", true_str(myargs.deleteyn));
			do_debug("myargs.do_modereader = %s\n", true_str(myargs.do_modereader));
			do_debug("myargs.portnr = %d\n", myargs.portnr);
			do_debug("myargs.host = %s\n", null_str(myargs.host));
			do_debug("myargs.phrases = %s\n", null_str(myargs.phrases));
			do_debug("myargs.auth.autoauth = %s\n", true_str(myargs.auth.autoauth));
			do_debug("myargs.rnews_file = %s\n", null_str(myargs.rnews_file));
			do_debug("myargs.rnews_path = %s\n", null_str(myargs.rnews_path));
			do_debug("myargs.show_name = %s\n", true_str(myargs.show_name));
			do_debug("myargs.ignore_readonly = %s\n", true_str(myargs.ignore_readonly));
			do_debug("myargs.do_ssl = %s\n", true_str(myargs.do_ssl));
#ifdef TIMEOUT
			do_debug("TimeOut = %d\n", TimeOut);
#endif
			do_debug("myargs.debug = TRUE\n");
		}

		

		/* we processed args okay */
		myargs.sockfd = connect_to_nntphost( myargs.host, &hi, myargs.status_fptr, myargs.portnr, myargs.do_ssl, &myargs.ssl_struct);
		if(myargs.sockfd < 0) {
			retval = RETVAL_ERROR;
		}
		else {
			/* Get the announcement line */
			if((i = sgetline(myargs.sockfd, &inbuf, myargs.do_ssl, myargs.ssl_struct)) < 0) {
				retval = RETVAL_ERROR;
			}
			else {
				fputs(inbuf, myargs.status_fptr );
				number(inbuf, &response);
				if(response == 480) {
					retval = do_authenticate(&myargs);
				}
			}
			if(retval == RETVAL_OK && myargs.do_modereader == TRUE) {
				retval = send_command(&myargs, "mode reader\r\n", &inbuf, 0);
				if( retval  == RETVAL_OK) {				
					/* Again the announcement */
					fputs(inbuf, myargs.status_fptr );
					number(inbuf, &response);
				}
			}
			if(response == 201 && myargs.ignore_readonly == FALSE) {
			/*	if((response != 200) && (response != 480)) {*/
				error_log(ERRLOG_REPORT, rpost_phrases[3], NULL);
				retval = RETVAL_ERROR;
			}
			else if(myargs.auth.autoauth == TRUE) {
				if(myargs.auth.passwd == NULL || myargs.auth.userid == NULL) {
					error_log(ERRLOG_REPORT, rpost_phrases[32], NULL);
					retval = RETVAL_ERROR;
				}
				else {
					retval = do_authenticate(&myargs);
				}
			}
		}
		if(retval == RETVAL_OK) {
			if(myargs.rnews_file != NULL) {
				/* rnews mode */
				retval = do_rnews(&myargs);
			}
			else if(myargs.batch != NULL) {
				/* batch mode */
				retval = do_batch(&myargs);
			}
			else {
				/* do one article from stdin */
				retval = do_article(&myargs, stdin);
			}
	
			print_phrases(myargs.status_fptr, rpost_phrases[4], hi->h_name, NULL);
			if(myargs.debug == TRUE) {
				do_debug("Sending quit");
			}
			send_command(&myargs, "quit\r\n", NULL, 0);
			disconnect_from_nntphost(myargs.sockfd, myargs.do_ssl, &myargs.ssl_struct);
		}
	}
#ifdef PERL_EMBED
	if(myargs.perl_int != NULL) {
		perl_done(&myargs);
	}
#endif		
	if(myargs.status_fptr != NULL && myargs.status_fptr != stdout) {
		fclose(myargs.status_fptr);
	}
	free_phrases();	/* do this last so everything is displayed correctly */
	exit(retval);
}
/*----------------------------------------------------------------------------------*/
int do_article(Pargs myargs, FILE *fptr) {

	int len, response, retval, longline, dupeyn, i;
	char buf[MAXLINLEN+4], *inbuf, *ptr;
	/* the +4 in NL, CR, if have to double . in first pos */
	retval = RETVAL_OK;
	longline = FALSE;

	/* Initiate post mode don't use IHAVE since I'd have to scan the message for the msgid, */
	/* then rewind and reread the file The problem lies in if I'm using STDIN for the message, */
	/* I can't rewind!!!!, so no IHAVE */
	response = send_command(myargs, "POST\r\n", &inbuf, 340);
	fputs(inbuf, myargs->status_fptr);
	if(response!=RETVAL_OK) {
		error_log(ERRLOG_REPORT, rpost_phrases[5], NULL);
		retval = RETVAL_ERROR;
	}
	else {
		while(fgets(buf, MAXLINLEN, fptr) != NULL) {
			len=strlen(buf);
			if(longline == FALSE) {
				/* only check this if we are at the beginning of a line */
				if(buf[0]=='.')  {
				/* Yup, this has to be doubled */
					memmove(buf+1,buf,++len);
					buf[0]='.';
   				}
			}
			if(buf[len-1] == '\n') {
				/* only do this if we have an EOL in buffer */
				if(buf[len-2] != '\r') {
					/* only add the \r\n if it's not already there. */
					strncpy(&buf[len-1],"\r\n",3);
				}
				longline = FALSE;
			}
			else {
				longline = TRUE;
			}
   			sputline(myargs->sockfd, buf, myargs->do_ssl, myargs->ssl_struct);
			if(myargs->debug == TRUE) {
				do_debug("ARTICLE: %s", buf);
			}
		}
		/* if the last line didn't have a nl on it, we need to */
		/* put one so that the eom is recognized */
		if(longline == TRUE) {
			sputline(myargs->sockfd, "\r\n", myargs->do_ssl, myargs->ssl_struct);
		}
		sputline(myargs->sockfd, ".\r\n", myargs->do_ssl, myargs->ssl_struct);
		if(myargs->debug == TRUE) {
			do_debug("ARTICLE END\n");
		}

		if((i = sgetline(myargs->sockfd, &inbuf, myargs->do_ssl, myargs->ssl_struct)) < 0) {
			retval = RETVAL_ERROR;
		}
		else {
			fputs(inbuf, myargs->status_fptr);
			if(myargs->debug == TRUE) {
				do_debug("RESPONSE: %s", inbuf);
			}
			ptr = number(inbuf, &response);
		}
	 	if(retval == RETVAL_OK && response!=240) {
			dupeyn = FALSE;
			if(response == 441) {
				number(ptr, &response);
			}
			if(response == 435) { 
				dupeyn = TRUE;	
			}
			else {
				/* M$ server sends "441 (615) Article Rejected -- Duplicate Message ID" handle that */
				/* can't just call nr, cause of parens */
				/* ptr should be at ( after 441 */
				if( *ptr == '(' ) {
					number(++ptr, &response);
					if(response == 615) {
						dupeyn = TRUE;
					}		
				}
				else if (strstr(inbuf, RPOST_DUPE_STR) != NULL || strstr(inbuf, RPOST_DUPE_STR2)) {
					dupeyn = TRUE;
				}
			}
			if(dupeyn == TRUE) {
				print_phrases(myargs->status_fptr, rpost_phrases[6], NULL);
			}
			else {
				error_log(ERRLOG_REPORT, rpost_phrases[7], inbuf, NULL);
				retval = RETVAL_ARTICLE_PROB;
			}
		}
	}
	return retval;
}
/*----------------------------------------------------------------*/
int send_command(Pargs myargs, const char *cmd, char **ret_response, int good_response) {
	/* this is needed so can do user authorization */

	int len, retval = RETVAL_OK, nr;
	char *resp;

	if(myargs->debug == TRUE) {
		do_debug("sending command: %s", cmd);
	}
	sputline(myargs->sockfd, cmd, myargs->do_ssl, myargs->ssl_struct);
	len = sgetline(myargs->sockfd, &resp, myargs->do_ssl, myargs->ssl_struct);	
	if( len < 0) {	
		retval = RETVAL_ERROR;		  	
	}					
	else {
	  if(myargs->debug == TRUE) {
	    do_debug("got answer: %s", resp);
	  }

	  number(resp, &nr);
	  if(nr == 480 ) { 
		  /* we must do authorization */
		  retval = do_authenticate(myargs);
		  if(retval == RETVAL_OK) {
			  /* resend command */
			  sputline(myargs->sockfd, cmd, myargs->do_ssl, myargs->ssl_struct);
			  if(myargs->debug == TRUE) {
				  do_debug("sending command: %s", cmd);
			  }
			  len = sgetline(myargs->sockfd, &resp, myargs->do_ssl, myargs->ssl_struct);
			  if( len < 0) {	
				  retval = RETVAL_ERROR;		  	
			  }
			  else {
				  number(resp,&nr);
				  if(myargs->debug == TRUE) {
					  do_debug("got answer: %s", resp);
				  }
			  }
		  }
	  }
	  if (good_response != 0 && nr != good_response) {
	    error_log(ERRLOG_REPORT, rpost_phrases[18],cmd,resp,NULL);
	    retval = RETVAL_UNEXPECTEDANS;
	  }
	}
	if(ret_response != NULL) {
		*ret_response = resp;
	}
	return retval;
}
/*----------------------------------*/
/* authenticate when receive a 480 */
/*----------------------------------*/
int do_authenticate(Pargs myargs) {
	 int len, nr, retval = RETVAL_OK;
	 char *resp, buf[MAXLINLEN];

	 /* we must do authorization */

	 print_phrases(myargs->status_fptr, rpost_phrases[41], NULL);
	 
	 sprintf(buf, "AUTHINFO USER %s\r\n", (myargs->auth).userid);
	 if(myargs->debug == TRUE) {
		 do_debug("sending command: %s", buf);
	 }
	 sputline(myargs->sockfd, buf, myargs->do_ssl, myargs->ssl_struct);
	 len = sgetline(myargs->sockfd, &resp, myargs->do_ssl, myargs->ssl_struct);
	 if( len < 0) {	
	   retval = RETVAL_ERROR;		  	
	 }					
	 else {
		 if(myargs->debug == TRUE) {
			 do_debug("got answer: %s", resp);
		 }

		 number(resp, &nr);
		 if(nr == 480) {
			 /* this is because of the pipelining code */
			 /* we get the second need auth */
			 /* just ignore it and get the next line */
			 /* which should be the 381 we need */
			 if((len = sgetline(myargs->sockfd, &resp, myargs->do_ssl, myargs->ssl_struct)) < 0) {
				 retval = RETVAL_ERROR;
			 }
			 else {
				 number(resp, &nr);
			 } 
		 }
		 if(retval == RETVAL_OK) {
			 if(nr != 381) {
				 error_log(ERRLOG_REPORT, rpost_phrases[16], resp, NULL);
				 retval = RETVAL_NOAUTH;
			 }
			 else {
				 sprintf(buf, "AUTHINFO PASS %s\r\n", (myargs->auth).passwd);
				 sputline(myargs->sockfd, buf, myargs->do_ssl, myargs->ssl_struct);
				 if(myargs->debug == TRUE) {
					 do_debug("sending command: %s", buf);
				 }
				 len = sgetline(myargs->sockfd, &resp, myargs->do_ssl, myargs->ssl_struct);
				 if(len < 0) {	
					 retval = RETVAL_ERROR;		  	
				 }					
				 else {
					 if(myargs->debug == TRUE) {
						 do_debug("got answer: %s", resp);
					 }
					 number(resp, &nr);
					 switch(nr) {
					 case 281: /* bingo */
						 retval = RETVAL_OK;
						 print_phrases(myargs->status_fptr, rpost_phrases[42], NULL);
						 break;
					 case 502: /* permission denied */
						 retval = RETVAL_NOAUTH;
						 error_log(ERRLOG_REPORT, rpost_phrases[17], NULL);
						 break;
					 default: /* wacko error */
						 error_log(ERRLOG_REPORT, rpost_phrases[16], resp, NULL);
						 retval = RETVAL_NOAUTH;
						 break;
					 }
				 }
			 }
		 }
		 
	 }
	 return retval;
}
/*--------------------------------------------------------------------------------------*/
int scan_args(Pargs myargs, int argc, char *argv[]) {

	int loop, retval = RETVAL_OK;
	
	for(loop=0;loop<argc && retval == RETVAL_OK;loop++) {
		if(myargs->debug == TRUE) {
			do_debug("Checking arg #%d-%d: '%s'\n", loop, argc, argv[loop]);
		}
		/* check for valid args format */
		if(argv[loop][0] != '-' && argv[loop][0] != FILE_CHAR) {
			retval = RETVAL_ERROR;
		}
		if(argv[loop][0] == '-' && argv[loop][2] != '\0') {
			retval = RETVAL_ERROR;
		}

		if(retval != RETVAL_OK) {
			error_log(ERRLOG_REPORT, rpost_phrases[19], argv[loop], NULL);
		}
		else if(argv[loop][0] != FILE_CHAR) {
			switch(argv[loop][1]) {
			  case 'h': 	/* hostname */
				if(loop+1 == argc) {
					error_log(ERRLOG_REPORT, rpost_phrases[20], NULL);
					retval = RETVAL_ERROR;
				}
				else { 
					myargs->host = argv[++loop];
				}
				break;
				
			  case 'b':	/* batch file Mode, next arg = batch file */
				if(loop+1 == argc) {
					error_log(ERRLOG_REPORT, rpost_phrases[21], NULL);
					retval = RETVAL_ERROR;
				}
				else { 
					myargs->batch = argv[++loop];
				}
				break;
			  case 'p':	/* specify directory prefix for files in batch file */
				if(loop+1 == argc) {
					error_log(ERRLOG_REPORT, rpost_phrases[22], NULL);
					retval = RETVAL_ERROR;
				}
				else {
					myargs->prefix = argv[++loop];
				}
				break;
			  case 'f':	/* filter prg, rest of cmd line is args */
				if(loop+1 == argc) {
					error_log(ERRLOG_REPORT, rpost_phrases[23], NULL);
					retval = RETVAL_ERROR;
				}
				else {
					/* set up filter args send it first arg to filter and arg count */
					retval = parse_filter_args(myargs, argc - (loop+1), &(argv[loop+1]));
					loop = argc;		/* terminate main loop */
				}
				break;
#ifdef PERL_EMBED
			  case 'F': /* filter using embedded perl */
				if(loop+1 == argc) {
					error_log(ERRLOG_REPORT, rpost_phrases[33], NULL);
				}
				else {
					/* parse the perl filter */
					parse_perl(myargs, argv[++loop]);
				}
				break;
#endif	  
			  case 'e':	/* use default error log path */
				error_log(ERRLOG_SET_FILE, ERROR_LOG,NULL);
				break;
			  case 'E':	/* error log path */
				if(loop+1 == argc) { 
					error_log(ERRLOG_REPORT, "%s\n",rpost_phrases[24],NULL);
					retval = RETVAL_ERROR;
				}
				else {
					error_log(ERRLOG_SET_FILE, argv[++loop], NULL);
				}
				break;
		  	  case 's':	/* use default status log path */
				if((myargs->status_fptr = fopen(STATUS_LOG, "a")) == NULL) {
					MyPerror(rpost_phrases[25]);
					retval = RETVAL_ERROR;
				}
				break;
			  case 'S':	/* status log path */
				if(loop+1 == argc) { 
					error_log(ERRLOG_REPORT, rpost_phrases[26],NULL);
					retval = RETVAL_ERROR;
				}	
				else if((myargs->status_fptr = fopen(argv[++loop], "a")) == NULL) {
					MyPerror(rpost_phrases[25]);
					retval = RETVAL_ERROR;
				}
				break;
			  case 'd':	/* delete batch file on success */
				myargs->deleteyn = TRUE;
				break;
			  case 'u':	/* do auto-authenticate */
				myargs->auth.autoauth = TRUE;
				break;
			  case 'U': 	/* next arg is userid for authorization */
				if(loop+1 == argc) {
					error_log(ERRLOG_REPORT, rpost_phrases[27],NULL);
					retval = RETVAL_ERROR;
				}
				else { 
					myargs->auth.userid = argv[++loop];
				}
				break;
			  case 'P': 	/* next arg is password for authorization */
				if(loop+1 == argc) {
					error_log(ERRLOG_REPORT, rpost_phrases[28],NULL);
					retval = RETVAL_ERROR;
				}
				else { 
					myargs->auth.passwd = argv[++loop];
				}
				break;
			  case 'M':	/* do mode reader command */
				myargs->do_modereader = TRUE;
				break;
			  case 'N':	/* override port number */
				if(loop+1 == argc) {
					error_log(ERRLOG_REPORT, rpost_phrases[29],NULL);
					retval = RETVAL_ERROR;
				}
				else {
					myargs->portnr = atoi(argv[++loop]);
				}
				break;
			  case 'l': 	/* language  phrase file */
				if(loop+1 == argc) {
					error_log(ERRLOG_REPORT, rpost_phrases[31],NULL);
					retval = RETVAL_ERROR;
				}
				else {
					myargs->phrases = argv[++loop];
				}
				break;
			  case 'D':	/* debug */
				myargs->debug = TRUE;
				/* so error_log goes to debug too */
				error_log(ERRLOG_SET_DEBUG, NULL, NULL);
				break;
			case 'r':      /* do rnews next args  base name then path for the rnews files */
				if(loop+2 >= argc) {
					error_log(ERRLOG_REPORT, rpost_phrases[38], NULL);
					retval = RETVAL_ERROR;
				}
				else {
					myargs->rnews_file = argv[++loop];
					myargs->rnews_path = argv[++loop];
				}
				break;
			case 'n':  /* show the file name as we're uploading it */
				myargs->show_name = TRUE;
				break;
			case 'i':  /* ignore read_only opening announcement */
				myargs->ignore_readonly = TRUE;
				break;
#ifdef TIMEOUT
			case 'T': /* timeout */
				if(loop+1 == argc) {
					error_log(ERRLOG_REPORT, rpost_phrases[31], NULL);
					retval = RETVAL_ERROR;
				}
				else {
					TimeOut = atoi(argv[++loop]);
				}
				break;
#endif
#ifdef HAVE_LIBSSL
			case 'z': /* use SSL */
				myargs->do_ssl = TRUE;
				myargs->portnr = DEFAULT_SSL_PORT;
				break;
#endif
			  default:
				error_log(ERRLOG_REPORT, rpost_phrases[30], argv[loop],NULL);
				break;
			}
		}
	}
	return retval;
}

#ifdef PERL_EMBED
/*-----------------------------------------------------------------------------------------*/
void parse_perl(Pargs args, char *fname) {

	/* this code is copied from killprg.c killperl_setup() */
	/* which comes from PERLEMBED man page */
	
	char *ptr = NULL;
	char *prgs[] = { ptr, ptr };
	
	if ((args->perl_int = perl_alloc()) == NULL) {
		error_log(ERRLOG_REPORT, rpost_phrases[34], NULL);
	}
	else {
		if(args->debug == TRUE) {
			do_debug("Perl program name =%s\n", fname);
		}
		perl_construct(args->perl_int);
		prgs[1] = fname;
		if(perl_parse(args->perl_int, NULL, 2, prgs, NULL) == 0) {
			perl_run(args->perl_int);
		}
		else {
			error_log(ERRLOG_REPORT, rpost_phrases[35], fname, NULL);
			perl_done(args);
		}
	} 
}
/*---------------------------------------------------------------------------------------*/
void perl_done(Pargs args) {
	/* all done, free everything up */
	if(args->perl_int != NULL) {
		perl_destruct(args->perl_int);
		perl_free(args->perl_int);
		args->perl_int = NULL;
	}
}
/*--------------------------------------------------------------------------------------*/
char *filter_perl(Pargs myargs, char *file) {

	 int i;
	 char *args[2] ={ NULL, NULL};
	 char infilen[MAXLINLEN+1];
	 char *infile=NULL;
	 dSP; /* perl stack pointer */
	 SV *fname; /* used to get fname off of perl stack */
	 
				/* here's where we send it to perl and get back our filename */
				/* we need infile to become our file name to post*/
				/* this code comes from chk_msg_kill_perl() which uses */
				/* both perlcall and perlembed man pages */
				/* set up args */
	 args[0] = file;

	 if(myargs->debug == TRUE) {
		 do_debug("Calling %s with arg %s\n", PERL_RPOST_SUB, file);
	 }
				
	 ENTER;
	 SAVETMPS;
	 PUSHMARK(SP);

	 i = perl_call_argv(PERL_RPOST_SUB, G_SCALAR | G_EVAL | G_KEEPERR, args);
	 
	 SPAGAIN;

	 if(SvTRUE(ERRSV)) {
		 error_log(ERRLOG_REPORT, rpost_phrases[36], SvPV(ERRSV,PL_na));
		 POPs;
		 perl_done(myargs);
	 }
	 else if(i != 1) {
		 error_log(ERRLOG_REPORT, rpost_phrases[37], PERL_RPOST_SUB, NULL);
		 perl_done(myargs);
	 }
	 else {
		 fname = POPs;
		 infile = SvPV(fname, PL_na);
		 if(myargs->debug == TRUE) {
			 do_debug("got string =%s\n", null_str(infile));
		 }
                 /* have to do the following cause as soon as we do FREETMPS, the */
                 /* data that infile points to will disappear */
		 strcpy(infilen, infile);
		 infile = infilen; /* so we return the name of the file */
	 }
	 PUTBACK;
	 FREETMPS;
	 LEAVE;

	 return infile;
	 
}
#endif
/*---------------------------------------------------------------------------------------*/
int parse_filter_args(Pargs myargs, int argcount, char **args) {
	 
	 int argon, loop, i, retval = RETVAL_OK;
	 
	 /* build filter args */ 
	 i = strlen(RPOST_FILTER_OUT);

	 /* just in case */
	 myargs->filter_outfile = NULL;
	 myargs->filter_infilenr = -1;
	 /* build array of args to past to fork, execl */
	 /* if see RPOST_FILTER_IN as arg, substitute the infile name */
	 /* if see RPOST_FILTER_OUT as first part of arg, get outfile name */
	 for(argon = 0, loop = 0; loop < argcount; loop++) {

		 if(myargs->debug == TRUE) {
			 do_debug("loop=%d argon=%d arg=%s\n", loop, argon, args[loop]);
		 }
		 
		 if(strcmp(args[loop], RPOST_FILTER_IN) == 0) {
			 /* substitute infile name */
			 myargs->filter_args[argon] = NULL; /* just so debug.suck looks okay */
			 myargs->filter_infilenr = argon++;
		 }
		 else if(strncmp(args[loop], RPOST_FILTER_OUT, (unsigned int) i) == 0) {
			 /* arg should be RPOST_FILTER_OUT=filename */
			 if(args[loop][i] != '=') {
				 error_log(ERRLOG_REPORT, rpost_phrases[17], args[loop], NULL);
			 }
			 else {
				 myargs->filter_outfile = (char *)&(args[loop][i+1]);
			 }
		 }
		 else {
			 myargs->filter_args[argon++] = args[loop];
		 }
	 }
	 myargs->filter_argc = argon; /* the count of arguments */
	 myargs->filter_args[argon] = NULL;  /* just to be on the safe side */
	 if(myargs->filter_outfile == NULL) {
                                /* no outfile defined, use built-in default */
		 myargs->filter_outfile = tmpnam(NULL);
		 error_log(ERRLOG_REPORT, rpost_phrases[9], myargs->filter_outfile, NULL);
	 }
	 if(myargs->filter_infilenr < 0) {
		 error_log(ERRLOG_REPORT, rpost_phrases[10], NULL);
		 retval = RETVAL_ERROR;
	 }
	 return retval;
}
/*--------------------------------------------------------------------*/
char *do_filter(Pargs myargs, char *filename) {
	char *infile = NULL;
	int i;
	
	myargs->filter_args[myargs->filter_infilenr] = filename; /* so this points to the right name */
	
	if( myargs->debug == TRUE) {
		do_debug("ARGS:");
		for(i=0;i<myargs->filter_argc;i++) {
			do_debug(" %d=%s", i, myargs->filter_args[i]);
		}
		do_debug("\n");
	}
	switch((int) fork()) {
	  case 0:	/* in child, do execl */
		if(execvp(myargs->filter_args[0], myargs->filter_args) == -1) {
			MyPerror(rpost_phrases[14]);
			exit(-1); /* so we don't get two running */
		}
		break;
	  case -1:	/* should never get here */
		MyPerror(rpost_phrases[15]);
		break;
	  default:	/* in parent, wait for child to finish */
		wait(NULL);		/* no status check on finish */
		infile = myargs->filter_outfile;
	}					

	return infile;
}
/*----------------------------------------------------------------------------------*/
int filter_post_article(Pargs myargs, char *filename) {

	 char *infile = NULL; /* this is the filename that gets passed to do_article() */
	 int retval = RETVAL_OK;
	 FILE *fpi_msg;
	 
#ifdef PERL_EMBED
	 if(myargs->perl_int != NULL) {
		 infile  = filter_perl(myargs, filename);
	 }		
	 else
#endif
		 if(myargs->filter_argc >0) {
			 infile = do_filter(myargs, filename);
		 }
		 else {
				/* so we open up the right file */
			 infile = filename;
		 } 
	 
	 if(infile == NULL) {
		 error_log(ERRLOG_REPORT, rpost_phrases[11], NULL);
		 retval = RETVAL_ERROR;
	 }
	 else {
		 if(myargs->show_name == TRUE) {
			 /* show the file name */
			 print_phrases(myargs->status_fptr, rpost_phrases[40], infile, NULL);
		 }
		 
		 if((fpi_msg = fopen(infile, "r")) == NULL) {
			 /* skip to next article if infile don't exist */
			 error_log(ERRLOG_REPORT, rpost_phrases[12],NULL);
		 }
		 else {
			 retval = do_article(myargs, fpi_msg);
			 if(myargs->debug == TRUE && retval != RETVAL_OK) {
				 do_debug("do_article() returned %d\n", retval);
			 }
			 fclose(fpi_msg);
		 }
	 }	
	 return retval;
}
/*--------------------------------------------------------------------------------*/
int do_batch(Pargs myargs) {

	int i, x, nrdone, nrok, retval;
	FILE *fpi_batch;
	char buf[MAXLINLEN+1], file[MAXLINLEN+1];
	char *outfile;

	outfile = NULL;
	retval = RETVAL_OK;
	nrdone = nrok = 0;

	if((fpi_batch = fopen(myargs->batch, "r")) == NULL) {
		MyPerror(myargs->batch);
		retval = RETVAL_ERROR;
	}
	else {		
		while((retval != RETVAL_ERROR) && (fgets(buf, MAXLINLEN, fpi_batch) != NULL)) {
			/* build file name */
			if(myargs->prefix == NULL) {
				strcpy(file, buf);
			}
			else {
				strcpy(file, myargs->prefix);
				if(file[strlen(file)-1] != '/') {
					strcat(file, "/");
				}
				strcat(file, buf);
			}
			/* strip off nl */
			i = strlen(file);
			if(file[i-1] == '\n') {
				file[i-1] = '\0';
			}
			/* some INNs put article number on line in addition to file name */
			/* so lets parse through string, if find a ' ' NULL terminate at */
			/* that point */
			for(x=0;x<i;x++) {
				if(file[x] == ' ') {
					file[x] = '\0';
					x = i;	/* to break loop */
				}
			}
			/* okay here have file name */
			if(myargs->debug == TRUE) {
				do_debug("Article Name: %s\n", file);
			}
			retval = filter_post_article(myargs, file);

			nrdone++;
			if(retval == RETVAL_OK) {
				nrok++;
			}
				/* log failed uploads (dupe article is not a failed upload ) */
			else if (retval == RETVAL_ARTICLE_PROB) {
				log_fail(myargs->batch, buf);
			}
		
		}
		if(retval == RETVAL_ERROR) {
			/* write out the rest of the batch file to the failed file */
			do {
				log_fail(myargs->batch, buf);
			} while (fgets(buf, MAXLINLEN, fpi_batch) != NULL);
		}	
	 	fclose(fpi_batch);

		if(retval != RETVAL_ERROR) {
			retval = (nrok == nrdone) ? RETVAL_OK : RETVAL_ARTICLE_PROB;
		}		
		if(myargs->deleteyn == TRUE && retval == RETVAL_OK) {
			unlink(myargs->batch);
			print_phrases(myargs->status_fptr, rpost_phrases[13], myargs->batch,NULL);
		}
	}

	return retval;
}
/*---------------------------------------------------------------*/
void log_fail(char *batch_file, char *article) {

	char file[MAXLINLEN+1];

	FILE *fptr;

	sprintf(file, "%s%s", batch_file, RPOST_FAIL_EXT);

	if((fptr = fopen(file, "a")) == NULL) {
		MyPerror(file);
	}
	else {
		fputs(article, fptr);
		fclose(fptr);
	}
}
/*------------------------------------------------------------------------------------------------*/
int do_rnews(Pargs myargs) {
	 /* handle rnews files */
	 int len, nrdone, nrok, retval = RETVAL_OK;
	 DIR *dptr;
	 FILE *f_rnews, *f_temp;
	 char rnews_path[PATH_MAX+1], temp_path[PATH_MAX+1], linein[MAXLINLEN];
	 struct dirent *dentry;

	 sprintf(temp_path, "%s/%s", myargs->rnews_path, TEMP_ARTICLE); /* only need to do this once */
	 
	 if(myargs->rnews_file == NULL || myargs->rnews_path == NULL) {
		 error_log(ERRLOG_REPORT, rpost_phrases[39], NULL);
		 retval = RETVAL_ERROR;
	 }
	 else if((dptr = opendir(myargs->rnews_path)) == NULL) {
		 error_log(ERRLOG_REPORT, rpost_phrases[39], NULL);
		 retval = RETVAL_ERROR;
	 }
	 else {
		 if(myargs->debug == TRUE) {
			 do_debug("Reading directory %s\n", myargs->rnews_path);
		 }
		 
		 len = strlen(myargs->rnews_file);
		 /* work our way thru the directory find our files that */
		 /* start with our rnews_file prefix */
		 while((dentry = readdir(dptr)) != NULL && retval != RETVAL_ERROR) {
			 if(strncmp(dentry->d_name, myargs->rnews_file, len) == 0) {
				 /* bingo, we've found one of the files*/
				 /* now work our way thru it and create our temp article */
				 /* which can be fed thru the filters and then to do_article() */
				 nrdone = nrok = 0; /* which article are we on */
				 sprintf(rnews_path, "%s/%s", myargs->rnews_path, dentry->d_name);
				 if(myargs->debug == TRUE) {
					 do_debug("Trying to read rnews file %s\n", rnews_path);
				 }
				 
				 if((f_rnews = fopen(rnews_path, "r")) == NULL) {
					 MyPerror(rnews_path);
					 retval = RETVAL_ERROR;
				 }
				 else {
					 f_temp = NULL;
					 /* now work our way thru the file */
					 while(retval != RETVAL_ERROR && fgets(linein, MAXLINLEN, f_rnews) != NULL) {
						 if(strncmp(linein, RNEWS_START, RNEWS_START_LEN) == 0) {
							 if(myargs->debug == TRUE) {
								 do_debug("Found rnews line %s", linein);
							 }	 
							 if(f_temp != NULL) {
								 /* close out the old file, and process it */
								 fclose(f_temp);
								 nrdone++;
								 retval = filter_post_article(myargs, temp_path);
								 if(retval == RETVAL_OK) {
									 nrok++;
								 }
							 }
							 if((f_temp = fopen(temp_path, "w")) == NULL) {
								 MyPerror(temp_path);
								 retval = RETVAL_ERROR;
							 }
						 }
						 else if(f_temp != NULL) {
							 /* write the line out to the file */
							 fputs(linein, f_temp);
						 } 
					 }
					 if(f_temp != NULL) {
						 /* close out our final article */
						 fclose(f_temp);
						 nrdone++;
						 retval = filter_post_article(myargs, temp_path);
						 if(retval == RETVAL_OK) {
							 nrok++;
						 }
					 }
					 fclose(f_rnews);
					 if(retval != RETVAL_ERROR) {
						 retval = (nrok == nrdone) ? RETVAL_OK : RETVAL_ARTICLE_PROB;
					 }		
					 if(myargs->deleteyn == TRUE && retval == RETVAL_OK) {
						 unlink(rnews_path);
						 print_phrases(myargs->status_fptr, rpost_phrases[13], rnews_path,NULL);
					 }
				 }
			 }
		 }
		 closedir(dptr);
	 }
	 
	 return retval;
} 

/*--------------------------------------------------------------------------------*/
/* THE strings in this routine is the only one not in the arrays, since           */
/* we are in the middle of reading the arrays, and they may or may not be valid.  */
/*--------------------------------------------------------------------------------*/
void load_phrases(Pargs myargs) {

	int error=TRUE;
	FILE *fpi;
	char buf[MAXLINLEN];

	if(myargs->phrases != NULL) {
		
		if((fpi = fopen(myargs->phrases, "r")) == NULL) {
			MyPerror(myargs->phrases);
		}
		else {
			fgets(buf, MAXLINLEN, fpi);
			if(strncmp( buf, SUCK_VERSION, strlen(SUCK_VERSION)) != 0) {
				error_log(ERRLOG_REPORT, "Invalid Phrase File, wrong version\n", NULL);
			}
			else if((both_phrases = read_array(fpi, NR_BOTH_PHRASES, TRUE)) != NULL &&
				(rpost_phrases = read_array(fpi, NR_RPOST_PHRASES, TRUE)) != NULL) {
				error = FALSE;
			}
		}
		fclose(fpi);
		if(error == TRUE) {
			/* reset back to default */
			error_log(ERRLOG_REPORT, "Using default Language phrases\n",NULL);
			rpost_phrases = default_rpost_phrases;
			both_phrases = default_both_phrases;
		}
	}		
}
/*--------------------------------------------------------------------------------*/
void free_phrases(void) {
		/* free up the memory alloced in load_phrases() */
		if(rpost_phrases != default_rpost_phrases) {
			free_array(NR_RPOST_PHRASES, rpost_phrases);
		}
		if(both_phrases != default_both_phrases) {
			free_array(NR_BOTH_PHRASES, both_phrases);
		}
		
}
