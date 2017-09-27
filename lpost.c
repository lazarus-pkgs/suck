#include <config.h>

#include <stdio.h>
#include <errno.h>	
#include <string.h>
#include "suck_config.h"

/* get news data from stdin and post it to the local server */

int main(int argc,char *argv[]) {
	FILE *pfp=NULL;
	const int max_len = 1024;
	char line[max_len];
	int count=0,verbose=0, retval=0;
	size_t len;

	if (argc>1)  {
		verbose=1;
	}

    while(fgets(line, max_len, stdin) != NULL && retval == 0) {
  		len=strlen(line);
		if (pfp == NULL) {
			if (verbose != 0) {
				printf("posting article %d\n", ++count);
			}
			pfp = popen(RNEWS, "w");
		}
		if(pfp == NULL) {
			perror("Error: cannot open rnews: ");
			retval = -1;
		}
		else if (line[0] == '.' && len == 1) {
			/* end of article */
			if (verbose != 0) {
				printf("end of article %d\n",count);
			}
			if(pfp != NULL) {
				pclose(pfp);
				pfp = NULL;
			}
		}
		else {
			(void) fput(line, pfp);
		}
	} /* end while */
	exit(retval);
}
