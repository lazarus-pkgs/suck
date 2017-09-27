#ifndef _SUCK_SUCKUTILS_H
#define _SUCK_SUCKUTILS_H 1
 
int checkdir(const char *);
const char *full_path(int, int, const char *);
int qcmp_msgid(const char *, const char *);
int cmp_msgid(const char *, const char *);
int move_file(const char *, const char *);

#ifdef LOCKFILE
int do_lockfile(PMaster);
#endif

enum { FP_SET, FP_GET, FP_SET_POSTFIX, FP_GET_NOPOSTFIX, FP_GET_POSTFIX}; /* get or set call for full_path() */
enum { FP_TMPDIR, FP_DATADIR, FP_MSGDIR, FP_NONE };	  /* which dir in full_path() */
#define FP_NR_DIRS 3					 /* this must match nr of enums */

#endif /* _SUCK_SUCKUTILS_H */
