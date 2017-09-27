#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#define LENGTHLEN 8
#define DEBUG 1

#ifdef DEBUG
#define N_DEBUG "debug.child"
void do_debug(const char *fmt, ...);
#endif

void main(int argc, char *argv[]) {

	char buf[4096];
	int len, i, nron;

	nron = 0;
	do {
		i = read(0, buf, LENGTHLEN);
		buf[LENGTHLEN] = '\0';
#ifdef DEBUG
		do_debug("Got %d bytes '%s'\n", i, buf);
#endif
		len = atoi(buf);
		if(len>0) {
			nron++;
			i = read(0, buf, len);
#ifdef DEBUG
			do_debug("Got %d bytes\n", i);
#endif
			if((nron % 2) == 0) {
				i = write(1, "0\n", 2);
#ifdef DEBUG
				do_debug("Wrote %d bytes\n", i);
#endif
			}
			else {
				i = write(1, "1\n", 2);
#ifdef DEBUG
				do_debug("Wrote %d bytes\n", i);
#endif
			}
		}
	}
	while (len > 0);
}
#ifdef DEBUG
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
#endif
