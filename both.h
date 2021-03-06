#ifndef _SUCK_BOTH_H
#define _SUCK_BOTH_H 1

#include <stdio.h> /* for FILE */
#include <sys/types.h>
#include <netdb.h> /* for struct hostent */
#include "suck_config.h" /* for debug etc stuff */

/* declarations */
int sgetline(int fd, char **sbuf, int, void *);
int sputline(int fd, const char *outbuf, int, void *);
int connect_to_nntphost(const char *host, char *, size_t, FILE *, unsigned short int, int, void **);
void disconnect_from_nntphost(int, int, void **);
char *number(char *sp, int *intPtr);
char *get_long(char *, long *);
struct hostent *get_hostent(const char *host);
void signal_block(int);
void error_log(int mode, const char *fmt, ...);
void MyPerror(const char *);
void free_args(int, char *[]);
char **build_args(const char *, int *);
char **read_array(FILE *, int, int);
void free_array(int, char **);
char *do_a_phrase(char []);
void print_phrases(FILE *, const char *, ...);
char *str_int(int);
char *str_long(long);
void do_debug(const char *, ...);
void do_debug_binary(int, const char *);
const char *null_str(const char *);
const char *true_str(int);

#define N_DEBUG "debug.suck"

enum { MYSIGNAL_SETUP, MYSIGNAL_BLOCK, MYSIGNAL_UNBLOCK, MYSIGNAL_ADDPIPE };
enum { ERRLOG_SET_FILE, ERRLOG_SET_STDERR, ERRLOG_REPORT, ERRLOG_SET_DEBUG };

#define SOCKET_PROTOCOL 0	/* so testhost.c can get at it */

#ifndef FALSE
#define FALSE 0
#define TRUE !FALSE
#endif

#ifdef TIMEOUT
extern int TimeOut;		/* used to pass TimeOut value to sgetline() */
#endif

#ifdef HAVE_SOCKS_H
#include <socks.h>
#endif

#endif /* _SUCK_BOTH_H */
