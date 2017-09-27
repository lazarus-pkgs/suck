#include <config.h>

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "suck.h"

/* this file reads in the suck.db file and writes it out in a more readable format */

int main(int argc, char *argv[]) {

	int fdin;
	const char *fin = NULL, *fout = NULL;
	FILE *fpout;
	List item;
	Groups grp;
	long count;
	int retval = 0;
	
	if(argc == 1) {
		fin = "suck.db";
		fout = "suck.db.out";
	}
	else if(argc != 3) {
		fprintf(stderr, "Usage: %s in_file out_file <RETURN>\n", argv[0]);
		retval = -1;
	}
	else {
		fin = argv[1];
		fout = argv[2];
	}
	
	if(retval == 0) {
		if((fdin = open(fin, O_RDONLY)) == -1) {
			perror(fin);
			retval = -1;
		}
		else if((fpout = fopen(fout, "w")) == NULL) {
			perror(fout);
			retval = -1;
		}
		else {
			/* read items */
			read(fdin, &count, sizeof(long)); /* get item count */
			fprintf(fpout, "%ld\n", count);

			while(count > 0 && read(fdin, &item, sizeof(item)) == sizeof(item)) {
				fprintf(fpout, "%s-%d-%ld-%ld-%c-%d-%d-%d\n", item.msgnr, item.groupnr,
					item.nr,item.dbnr,item.mandatory,item.downloaded,
					item.delete,item.sentcmd);
				count--;
			}
			if(count >  0) {
				perror(fin);
				retval = -1;
			}
			else {
				read(fdin, &count, sizeof(long)); /* get group count */
				fprintf(fpout, "%ld\n", count);
				
				while(count >= 0 && read(fdin, &grp, sizeof(grp)) == sizeof(grp)) {
					fprintf(fpout, "%s-%d\n", grp.group, grp.nr);
					count--;
				}
				if(count >= 0) {
					perror(fin);
					retval = -1;
				}
			}
		}
		
	}
	return retval;
}
