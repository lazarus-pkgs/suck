#include <config.h>

#include <stdio.h>
#include <stdlib.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#include <sys/types.h>
#endif

#include <netdb.h>
#include <ctype.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_NET_SOCKET_H
#include <net/socket.h>
#endif
#include <errno.h>
#include <stdarg.h>

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#ifdef DMALLOC
#include <dmalloc.h>
#endif

#include "suck_config.h"
#include "both.h"
#include "phrases.h"

#ifdef TIMEOUT
/*------------------------------------*/
int TimeOut = TIMEOUT;
/* yes, this is the lazy way out, but */
/* there are too many routines that   */
/* call sgetline() to modify em all   */
/*------------------------------------*/
# if TIME_WITH_SYS_TIME
#  include <sys/time.h>
#  include <time.h>
# else
#  if HAVE_SYS_TIME_H
#    include <sys/time.h>
#  else
#    include <time.h>
#  endif
# endif
#endif /* TIMEOUT */

#ifdef MYSIGNAL
#include <signal.h>
#endif

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>		/* for aix */
#endif

#ifdef HAVE_LIBSSL
#include <openssl/ssl.h>
#endif

/* internal function proto */
void vprint_phrases(FILE *, const char *, va_list);
void do_debug_vl(const char *, va_list);
void convert_nl(char *);
char *findnl(char *, char *);

/*-----------------------------------------------------*/
/* get next number in string */
char *number(char *sp, int *intPtr) {
	int start, end;
	char c;
	char *retval;

	if(sp==NULL) {
		*intPtr=0;
		retval = sp;
	}
	else {
		/* skip any leading spaces */
		start = 0;
		while(sp[start] == ' ') {
			start++;
		}
		end = start;
		while(isdigit(sp[end])) {
			end++;
		}
		/* now we have the numbers width*/

		c=sp[end];	/* save off the character */
		sp[end]='\0';	/* truncate nr so sscanf works right */
		sscanf(&sp[start],"%d",intPtr);
		sp[end]=c;	/* restore it back */

		/* if at EOS return the NULL, else skip space */
		retval = (sp[end] == '\0') ? sp+end : sp+(++end);
	}
	return retval;
}
/*----------------------------------------------------*/
/* identical to above, except it gets a long vice int */
char *get_long(char *sp, long *intPtr) {
	int start, end;
	char c;
	char *retval;

	if(sp==NULL) {
		*intPtr=0;
		retval = sp;
	}
	else {
		/* skip any leading spaces */
		start = 0;
		while(sp[start] == ' ') {
			start++;
		}
		end = start;
		while(isdigit(sp[end])) {
			end++;
		}
		/* now we have the numbers width*/

		c=sp[end];	/* save off the character */
		sp[end]='\0';	/* truncate nr so sscanf works right */
		sscanf(&sp[start],"%ld",intPtr);
		sp[end]=c;	/* restore it back */

		/* if at EOS return the NULL, else skip space */
		retval = (sp[end] == '\0') ? sp+end : sp+(++end);
	}
	return retval;
}

/*---------------------------------------------*/
struct addrinfo *get_addrinfo(const char *host, const char *sport) {
	struct addrinfo hints = { .ai_socktype=SOCK_STREAM, .ai_flags = AI_CANONNAME };
	struct addrinfo * res = NULL;

	if(host==NULL) {
		error_log(ERRLOG_REPORT,both_phrases[0], NULL);
	}
	else {
		int st = getaddrinfo(host, sport, &hints, &res);
		if (st < 0) {
			error_log(ERRLOG_REPORT, "%v1%: %v2%: %v3%\n", host, both_phrases[2], gai_strerror(st), NULL);
		}
	}
	return res;
}
/*--------------------------------------------*/
int connect_to_nntphost(const char *host, char * name, size_t namelen, FILE *msgs, unsigned short int portnr, int do_ssl, void **ssl) {
	char *realhost;
	char sport[10];
	int sockfd = -1;
	struct addrinfo * ai;
	char buffer[60]; // if not given by caller. NI_MAXHOST would be better, but that's ok as well.

	if (host == NULL) {
		error_log(ERRLOG_REPORT, both_phrases[0], NULL);
		return sockfd;
	}

#ifdef HAVE_LIBSSL
	SSL *ssl_struct = NULL;
	SSL_CTX *test1 = NULL;

	if(do_ssl == TRUE) {
		(void) SSL_library_init();
		test1 = SSL_CTX_new(SSLv23_client_method());
		if(test1 == NULL) {
			/* whoops */
			error_log(ERRLOG_REPORT, both_phrases[18], NULL);
			return sockfd;
		}
	}
#endif
	if (!name) {
		name = buffer;
		namelen = sizeof buffer;
	}
	/* handle host:port type syntax */
	realhost = strdup(host);
	if(realhost == NULL) {
		MyPerror("out of memory copying host name");
		return sockfd;
	}
	char * ptr = strchr(realhost, ':');
	if(ptr != NULL) {
		*ptr = '\0';  /* null terminate host name */
		portnr = atoi(++ptr); /* get port number */
	}



	sprintf(sport, "%hu", portnr);	/* cause print_phrases wants all strings */
	print_phrases(msgs, both_phrases[1], sport, NULL);

	/* Find the internet addresses of the NNTP server */
	ai = get_addrinfo(realhost, sport);
	if(ai == NULL) {
		free(realhost);
	}
	else {
		free(realhost);
		struct addrinfo * aii;
		print_phrases(msgs, both_phrases[3], ai->ai_canonname, NULL);
		for (aii=ai; aii; aii=aii->ai_next) {
			if (getnameinfo(aii->ai_addr, aii->ai_addrlen, name, namelen, NULL, 0, NI_NUMERICHOST) < 0) {
				name[0] = '\0';
			}
			/* Create a socket */
			if((sockfd = socket(aii->ai_family, aii->ai_socktype, aii->ai_protocol)) == -1) {
				continue; // MyPerror(both_phrases[6]);
			}
			else {
				/* Establish a connection */
				if(connect(sockfd, aii->ai_addr, aii->ai_addrlen ) == -1) {
					//MyPerror(both_phrases[7]);
					close(sockfd);
					sockfd = -1;
					continue;
				}
				else {
					int st = getnameinfo(aii->ai_addr, aii->ai_addrlen, name, namelen,
						NULL, 0, NI_NUMERICHOST);
					print_phrases(msgs, both_phrases[8],st == 0 ? name : host, NULL);
					if (st != 0) name[0] = '\0';
					break;
				}
			}
		}
		freeaddrinfo(ai);
		if (sockfd < 0) {
			MyPerror(both_phrases[6]); // or 7?
		}
#ifdef HAVE_LIBSSL
		if(sockfd > -1 && do_ssl == TRUE) {
			if((ssl_struct = SSL_new(test1)) == NULL) {
				error_log(ERRLOG_REPORT, both_phrases[18], NULL);
				close(sockfd);
				sockfd = -1;
			}
			else if(SSL_set_fd(ssl_struct, sockfd) == FALSE) {
				error_log(ERRLOG_REPORT, both_phrases[18], NULL);
				close(sockfd);
				sockfd = -1;
			}
			else if(SSL_connect(ssl_struct) != 1) {
				error_log(ERRLOG_REPORT, both_phrases[18], NULL);
				close(sockfd);
				sockfd = -1;
			}
			else {
				*ssl = ssl_struct;
			}

		}
#endif
	}
	return sockfd;
}
/*---------------------------------------------------------------*/
void disconnect_from_nntphost(int fd, int do_ssl, void **ssl) {
#ifdef HAVE_LIBSSL
	if(do_ssl == TRUE) {
		fd = SSL_get_fd(*ssl);
		SSL_shutdown(*ssl);
		SSL_free(*ssl);
		*ssl = NULL;
	}
#endif
	close(fd);

}
/*----------------------------------------------------------------*/
int sputline(int fd, const char *outbuf, int do_ssl, void *ssl_buf) {

#ifdef DEBUG1
	do_debug("\nSENT: %s", outbuf);
#endif
#ifdef HAVE_LIBSSL
	if(do_ssl == TRUE) {
		if(fd == SSL_get_fd((SSL *)ssl_buf)) {
			return SSL_write((SSL *)ssl_buf, outbuf, strlen(outbuf));
		}
		else {
			return -1;
		}
	}
#endif
	return send(fd, outbuf, strlen(outbuf), 0);

}
/*-------------------------------------------------------------*/
void do_debug(const char *fmt, ...) {

	FILE *fptr = NULL;
	va_list args;

	if((fptr = fopen(N_DEBUG, "a")) == NULL) {
		fptr = stderr;
	}

	va_start(args, fmt);
	vfprintf(fptr, fmt, args);
	va_end(args);

	if(fptr != stderr) {
		fclose(fptr);
	}
}
/*------------------------------------------------------------*/
void do_debug_binary(int len, const char *str) {

	FILE *fptr = NULL;

	if((fptr = fopen(N_DEBUG, "a")) == NULL) {
		fptr = stderr;
	}
	fwrite(str, sizeof(str[0]), len, fptr);
	if(fptr != stderr) {
		fclose(fptr);
	}
}
/*-----------------------------------------------------------*/
void do_debug_vl(const char *fmt, va_list args) {
	FILE *fptr = NULL;

	if((fptr = fopen(N_DEBUG, "a")) == NULL) {
		fptr = stderr;
	}
	vprint_phrases(fptr, fmt, args);

	if(fptr != stderr) {
		fclose(fptr);
	}
}

/*------------------------------------------------------------*/
void MyPerror(const char *message) {

	/* can't just use perror, since it goes to stderr */
	/* and I need to route it to my errlog */
	/* so I have to recreate perror's format */

	/* in case of NULL ptr */

	if(message == NULL) {
		message="";
	}
#ifdef HAVE_STRERROR
	error_log(ERRLOG_REPORT, "%v1%: %v2%\n", message, strerror(errno), NULL);
#else
	error_log(ERRLOG_REPORT, both_phrases[9], message, errno, NULL);
#endif
}
/*-----------------------------------------------------------*/
char *findnl(char *startbuf, char *endbuf) {
	/* find a \r\n combo in the buffer */

	for(; startbuf < endbuf ; startbuf++) {
		if(*startbuf == '\r') {
			if(*(startbuf+1) == '\n' && (startbuf < endbuf-1)) {
				return startbuf;
			}
		}
	}
	return NULL;
}
/*-----------------------------------------------------------*/
int sgetline(int fd, char **inbuf, int do_ssl, void *ssl_buf) {

	static char buf[MAXLINLEN+MAXLINLEN+6];
	static char *start = buf;
	static char *eob = buf;		/* end of buffer */
	int ret, i, len;
	char *ptr;

#ifdef TIMEOUT
	fd_set myset;
	struct timeval mytimes;
#endif
	ret = 0;
	ptr = NULL;

	if(fd < 0) {
		ret = -1;
	}
	else if(eob == start || (ptr = findnl(start, eob)) == NULL) {

		/* TEST for not a full line in buffer */
		/* the eob == start test is needed in case the buffer is */
		/* empty, since we don't know what is in it. */

		len = eob-start; 	/* length of partial line in buf */
		if((eob - buf) > MAXLINLEN) {
#ifdef DEBUG1
			do_debug("SHIFTING BUFFER\n");
#endif
			/* not enuf room in buffer for a full recv */
			memmove(buf, start, len);		/* move to start of buf */
			eob = buf + len;
			*eob = '\0';
			start = buf;			/* reset pointers */
		}
		/* try to get a line in, up to maxlen */
		do {
#ifdef DEBUG1
			do_debug("\nCURRENT BUF start = %d, end = %d, len = %d\n", start - buf, eob - buf, len);
#endif
#ifdef TIMEOUT
			/* handle timeout value */
			FD_ZERO(&myset);
			FD_SET(fd, &myset);
			mytimes.tv_sec = TimeOut;
			mytimes.tv_usec = 0;

#ifdef MYSIGNAL
			signal_block(MYSIGNAL_BLOCK);
			/* block so we can't get interrupted by our signal defined in config.h */
#endif
#ifdef HAVE_LIBSSL
			if(do_ssl == TRUE && fd == SSL_get_fd((SSL *)ssl_buf) && SSL_pending((SSL *)ssl_buf)) {
				i = 1;
			}
			else {
#endif
			/* the fd+1 so we only scan our needed fd not all 1024 in set */
			i = select(fd+1, &myset, (fd_set *) NULL, (fd_set *) NULL, &mytimes);
#ifdef HAVE_LIBSSL
			}
#endif
			if(i>0) {
#ifdef HAVE_LIBSSL
#ifdef DEBUG1
				do_debug("SELECT got: %d\n", i);
#endif
				if(do_ssl == TRUE) {
					if(fd == SSL_get_fd((SSL *)ssl_buf)) {
						i = SSL_read((SSL *)ssl_buf, eob, MAXLINLEN-len);
					}
					else {
						i = -1;
					}
				}
				else
#endif
				i = recv(fd, eob, MAXLINLEN-len, 0);	/* get line */
			}
			else if(i == 0) {
				error_log(ERRLOG_REPORT, both_phrases[10], NULL);
			}      /* other errors will be handled down below */
#else
#ifdef HAVE_LIBSSL
			if(do_ssl == TRUE) {
				if(fd == SSL_get_fd((SSL *)ssl_buf)) {
					i = SSL_read((SSL *)ssl_buf, eob, MAXLINLEN-len);
				}
				else {
					i = -1;
				}
			}
			else
#endif
			i = recv(fd, eob, MAXLINLEN-len, 0);	/* get line */
#endif

#ifdef MYSIGNAL
			signal_block(MYSIGNAL_UNBLOCK);
			/* we are done, now unblock it */
#endif
#ifdef DEBUG1
			do_debug("\nRECV returned %d", i);
			if ( i > 0) {
				do_debug("\nGOT: ");
				do_debug_binary(i,eob);
				do_debug(":END GOT");
			}

#endif

			if(i < 1) {
				if(i == 0) {
					/* No data read in either from recv or select timed out*/
					error_log(ERRLOG_REPORT, both_phrases[11], NULL);
				}
				else {
					MyPerror(both_phrases[12]);
				}
				ret = -1;
			}
			else {

				eob += i;	/* increment buffer end */
				*eob = '\0';	/* NULL terminate it  */

				len += i;
				ptr = findnl(start, eob);
			}
		} while(ptr == NULL && len < MAXLINLEN && ret == 0);
	}
	if(ptr != NULL) {
		/* we have a full line left in buffer */
		*ptr++ = '\n';	/* change \r\n to just \n */
		*ptr++ = '\0';	/* null terminate */
		*inbuf = start;
		ret = (ptr-1) - start;	/* length of string */
		start = ptr; 	/* skip \r\n */
	}
	else if(ret == 0) {
		/* partial line in buffer */
		/* null terminate */
		*eob = '\0';
		*inbuf = start;
		ret = (eob-1) - start;	/* length of string */
		start = ++eob;	/* point both past end */
	}
#ifdef DEBUG1
	if(ret > 0) {
		int flag = FALSE;
		char saveit = '\0';
		/* change nl to something we can see */
		if((*inbuf)[ret-1] == '\n') {
			flag = TRUE;
			(*inbuf)[ret-1] = '\\';
			(*inbuf)[ret] = 'n';
			saveit = (*inbuf)[ret+1];
			(*inbuf)[ret+1] = '\0';
		}
		do_debug("\nRETURNING len %d: %s\n", ret, *inbuf);
		/* put things back the way they were */
		if(flag == TRUE) {
			(*inbuf)[ret-1] = '\n';
			(*inbuf)[ret] = '\0';
			(*inbuf)[ret+1] = saveit;
		}
	}
	else {
		do_debug("\nRETURNING error, ret = %d", ret);
	}
#endif
	if(eob == start) {
		/* nothing left in buffer, reset pointers to start of buffer */
		/* to give us a full buffer to receive next data into */
		/* hopefully doing this will mean less buffer shifting */
		/* meaning faster receives */
		eob = start = buf;
#ifdef DEBUG1
		do_debug("EMPTY BUFFER, resetting to start\n");
		/* take this out when debugged */
		memset(buf, '\0', sizeof(buf));
#endif
	}

	return ret;
}
/*----------------------------------------------------*/
#ifdef MYSIGNAL
void signal_block(int action) {
#ifdef HAVE_SIGACTION
	static sigset_t blockers;
	static int do_block = FALSE;

	switch(action) {
		case MYSIGNAL_SETUP:	/* This must be called first */
			sigemptyset(&blockers);
			if(sigaddset(&blockers, MYSIGNAL) == -1 || sigaddset(&blockers,PAUSESIGNAL) == -1) {
				MyPerror(both_phrases[13]);
			}
			else {
				do_block = TRUE;
			}
			break;
		case MYSIGNAL_ADDPIPE:	/* add SIGPIPE for killprg.c */
			if(sigaddset(&blockers, SIGPIPE) == -1) {
				MyPerror(both_phrases[14]);
			}
			break;
		case MYSIGNAL_BLOCK:
			if(do_block == TRUE) {
				if(sigprocmask(SIG_BLOCK, &blockers, NULL) == -1) {
					MyPerror(both_phrases[13]);
				}
			}
			break;
		case MYSIGNAL_UNBLOCK:
			if(do_block == TRUE) {
				if(sigprocmask(SIG_UNBLOCK, &blockers, NULL) == -1) {
					MyPerror("Unable to unblock signal");
				}
			}
			break;
	}
#endif /* HAVE_SIGACTION */
}
#endif /* MYSIGNAL */
/*-------------------------------------------------------------------*/
void error_log(int mode, const char *fmt, ...) {

	/* if we have been passed a file, report all errors to that file */
	/* else report all errors to stderr */
	/* handle printf type formats, hence the varargs stuff */

	FILE *fptr = NULL;
	va_list args;
	static char errfile[PATH_MAX] = { '\0' };
	static int debug = FALSE;

	va_start(args, fmt);	/* set up args */

	switch(mode) {
	  case ERRLOG_SET_DEBUG:
		debug = TRUE;
		break;
	  case ERRLOG_SET_FILE:
		strcpy(errfile,fmt);
		break;
	  case ERRLOG_SET_STDERR:
		errfile[0] = '\0';
		break;
	  case ERRLOG_REPORT:
		if(errfile[0] == '\0' || (fptr = fopen(errfile, "a")) == NULL) {
			fptr = stderr;
		}
		vprint_phrases(fptr, fmt, args);
		if(debug == TRUE) {
			va_list args;
			va_start(args, fmt);
			do_debug_vl(fmt, args);
			va_end(args);
		}
		if(fptr != stderr) {
			fclose(fptr);
		}
		break;
	}
	va_end(args);		/* so we can return normally */
	return;
}
/*--------------------------------------------------------------------------*/
char **build_args(const char *fname, int *nrargs) {
	/* read a file and parse the args into an argv array */
	/* make two passes thru the file, 1st count them so can allocate array */
	/* then allocate each one individually */

	char **args = NULL;
	char linein[MAXLINLEN+1];
	int x, len, done, counter, argc = 0;
	FILE *fpi;

	if((fpi = fopen(fname, "r")) == NULL) {
		MyPerror(fname);
	}
	else {
		/* first pass, just count em */
		counter = 0;
		while(fgets(linein, MAXLINLEN, fpi) != NULL) {
			x = 0;
			done = FALSE;
			while(linein[x] != '\0' && done == FALSE) {
				while(isspace(linein[x])) {
					x++;		/* skip white space */
				}
				if(linein[x] == FILE_ARG_COMMENT || linein[x] == '\0') {
					done = TRUE;	/* skip rest of line */
				}
				else {
					counter++;	/* another arg */
					while(!isspace(linein[x]) && linein[x] != '\0') {
						x++;	/* skip rest of arg */
					}
				}
			}
		}
#ifdef DEBUG1
		do_debug("Counted %d args in %s\n", counter, fname);
#endif
		if((args = calloc( counter, sizeof(char *))) == NULL) {
			error_log(ERRLOG_REPORT, both_phrases[15], fname, NULL);
		}
		else {
			/* pass two  read em and alloc em*/
			fseek(fpi, 0L, SEEK_SET);	/* rewind the file for pass two */
			counter = 0;	/* start at 0 again */
			while(fgets(linein, MAXLINLEN, fpi) != NULL) {
				x = 0;
				done = FALSE;
				while(linein[x] != '\0' && done == FALSE) {
					while(isspace(linein[x])) {
						x++;		/* skip white space */
					}
					if(linein[x] == FILE_ARG_COMMENT || linein[x] == '\0') {
						done = TRUE;	/* skip rest of line */
					}
					else {
						/* have another arg */
						len = 1;
						while(!isspace(linein[x+len]) && linein[x+len] != '\0') {
							len++;	/* find length of arg */
						}
						if((args[counter] = calloc( len+1, sizeof(char))) == NULL) {
							error_log(ERRLOG_REPORT, both_phrases[16], NULL);
						}
						else {
							strncpy(args[counter], &linein[x], len);
							args[counter][len] = '\0';	/* ensure null termination */
#ifdef DEBUG1
							do_debug("Read arg #%d: '%s'\n", counter, args[counter]);
#endif
							counter++;
						}
						x += len;	/* go onto next one */
					}
				}
			}
			argc = counter;		/* total nr of args read in and alloced */
		}
		fclose(fpi);
	}

	*nrargs = argc;
	return args;
}
/*-----------------------------------------------------------------------------*/
/* free the memory allocated in build_args() */
void free_args(int argc, char *argv[]) {

	int i;
	for(i=0;i<argc;i++) {
		free(argv[i]);
	}
	free(argv);
}
/*--------------------------------------------------------------------------------*/
/* These are used by the load_phrases() stuff					  */
/* read_array() and do_a_phrase() don't use the stored phrases                    */
/* since they are used when the phrases are being loaded. 			  */
/*--------------------------------------------------------------------------------*/
char **read_array(FILE *fpi, int nr, int save_yn) {

	int i, retval = 0;
	char **array = NULL;
	char linein[MAXLINLEN+1];

	if(save_yn == TRUE && (array = calloc( nr, sizeof(char *))) == NULL) {
		error_log(ERRLOG_REPORT, "Out of memory reading phrases\n", NULL);
	}
	else {
		for(i=0;retval == 0 && i < nr; i++) {
			if(fgets(linein, MAXLINLEN, fpi) != NULL) {
				if(save_yn == TRUE) {
					if((array[i] = do_a_phrase(linein)) == NULL) {
						retval = -1;
					}
				}
			}
		}
	}

	if(retval == -1) {
		free_array(nr, array);
		array = NULL;
	}
	return array;
}
/*-------------------------------------------------------------------------------*/
void free_array(int n, char **arr) {
	int i;
	if(arr != NULL) {
		for(i=0;i<n;i++) {
			if(arr[i] != NULL) {
				free(arr[i]);
			}
		}
		free(arr);
	}
}
/*--------------------------------------------------------------------------------*/
char *do_a_phrase(char linein[]) {

	char *lineout;
	int i, stt, end;
	static char default_phrase[] = "";


	i = strlen(linein);
	lineout = NULL;

	/* find start and finish of string */
	for(stt = 0; stt != i && linein[stt] != '"'; stt++) {
	}
	for(end = i-1;  i!= 0 && linein[end] != '"'; end--) {
	}
	if(end <= stt) {
		error_log(ERRLOG_REPORT, "Invalid line, %v1%, Ignoring\n", linein, NULL);
		lineout = default_phrase;
	}
	else {
		i = end - stt;
		if((lineout = calloc(i, sizeof(char))) == NULL) {
			error_log(ERRLOG_REPORT, "Out of Memory Reading Phrases\n", NULL);
		}
		else {
			strncpy(lineout, &linein[stt+1], i-1);	/* put the string into place */
			lineout[i-1] = '\0';			/* null terminate it */
#ifdef DEBUG1
			do_debug("Got phrase:%s:\n", lineout);
#endif
			convert_nl(lineout);
		}
	}
	return lineout;
}
/*-----------------------------------------------------------------------------------*/
void convert_nl(char *linein) {
	/* go thru a string, and convert \r \t \n (2 chars) to actual control codes */
	char c;
	int i, x, len;

	if(linein != NULL) {
		len = strlen(linein);
		for(i = 0; i < len ; i++ ) {
			if(linein[i] == '\\') {
				c = linein[i+1];
				if(c == 'r' || c == 'n' || c == 't') {
					switch(c) {
					  case 'r':
						c = '\r';
						break;
					  case 'n':
						c = '\n';
						break;
					  case 't':
						c = '\t';
						break;
					}
					linein[i] = c;
					for ( x = i + 1; x < len ; x++ ) {
						linein[x] = linein[x+1];
					}
					len--;
				}
			}
		}
	}
}
/*-----------------------------------------------------------------------------------*/
void print_phrases(FILE *fpout, const char *argstr, ... ){

	va_list vargs;

	va_start(vargs, argstr);

	if(fpout != NULL) {
		/* if NULL don't need to print anything */
		vprint_phrases(fpout, argstr, vargs);
	}

	va_end(vargs);
}

/*-----------------------------------------------------------------------------------*/
void vprint_phrases(FILE *fpout, const char *argstr, va_list vargs) {
	/* this routine takes in argstr, parses it for args and prints everything */
	/* out, the args may not be in order ("hi %v2% there %v1% guys")	  */
	/* args are %v#%.  The varargs are all char ptrs */

	const char *ptr, *pnumber;
	char *tptr, block[PHRASES_BLOCK_SIZE+1], *in[PHRASES_MAX_NR_VARS];
	int len, varyn, vnr, nrvars;

	/* create an array with our string pointers */
	nrvars = 0;
	tptr = va_arg(vargs, char *);
	while(tptr != NULL && nrvars < PHRASES_MAX_NR_VARS) {
		in[nrvars++] = tptr;
		tptr = va_arg(vargs, char *);
	}

	varyn = FALSE;
	ptr = argstr;
	if(ptr != NULL) {
		while( *ptr != '\0' ) {
			len = 0;
			while(len < PHRASES_BLOCK_SIZE && *ptr != '\0') {
				if(*ptr == PHRASES_SEPARATOR && *(ptr+1) == PHRASES_VAR_CHAR) {
					pnumber = ptr+2;
					while(isdigit(*pnumber)) {
						pnumber++;
					}
					if(pnumber != ptr+2 && *pnumber == PHRASES_SEPARATOR) {
						/* bingo, we match syntax %v1% handle the variables */
						sscanf(ptr+2, "%d", &vnr);	/* get the number */
						/* now print it */
						if(vnr <= nrvars) {
							varyn = TRUE;
							/* first, print what's currently in the buffer */
							block[len] = '\0';
							fputs(block,fpout);
							len = 0;
							/* now print the variable */
							/* -1 cause array starts at 0 vars at 1 */
							fputs(in[vnr-1], fpout);
							ptr = pnumber+1;	/* advance past var */
						}
					}
				}
				if(varyn == FALSE) {
					block[len++] = *ptr++;
				}
				else {
					varyn = FALSE;		/* reset it */
				}
			}
			if(len > 0 ) {
				block[len] = '\0';	/* null terminate string */
				fputs(block, fpout);
			}
		}
	}
}
/*----------------------------------------------------------*/
char *str_int(int nrin) {

	static char strings[PHRASES_MAX_NR_VARS][12];	/* lets pray ints don't get bigger than this */
	static int which = 0;

	/* use a set of rotating buffers so can make multiple str_int calls */
	if(++which == PHRASES_MAX_NR_VARS) {
		which = 0;
	}

	sprintf(strings[which], "%d", nrin);

	return strings[which];
}
/*----------------------------------------------------------*/
char *str_long(long nrin) {

	static char strings[PHRASES_MAX_NR_VARS][20];	/* lets pray longs don't get bigger than this */
	static int which = 0;

	/* use a set of rotating buffers so can make multiple str_int calls */
	if(++which == PHRASES_MAX_NR_VARS) {
		which = 0;
	}

	sprintf(strings[which], "%ld", nrin);

	return strings[which];
}
/*-----------------------------------------------------------*/
/* print NULL or the string */
const char *null_str(const char *ptr) {
        const char *nullp = "NULL";

        return (ptr == NULL) ? nullp : ptr;
}
/*-----------------------------------------------------------*/
/* print TRUE or FALSE if our nr is 0 or non-zero */
const char *true_str(int nr) {
        const char *true_s = "TRUE";
        const char *false_s = "FALSE";

        return (nr == TRUE) ? true_s : false_s;
}
