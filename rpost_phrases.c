/* the default phrases */
const char *default_rpost_phrases[] = {
	"Usage: %v1% hostname [@filename] [-s | -S name] [-e | -E name] [-b batch_file -p dir_prefix]",	/* 0 */
	"-U userid -P passwd] [-M] [-N portnr] [-T timeout] [-u] [-f filter_args] <RETURN>\n",
	"\tIf used, filter_args MUST be last arguments on line\n",
	"Sorry, Can't post to this host\n",
	"Closing connection to %v1%\n",
	"Bad luck.  You can't use this server.\n",		/* 5 */
	"Duplicate article, unable to post\n",		
	"Malfunction, Unable to post Article!\n%v1%",
	"Invalid argument: %v1%\n",
	"Using Built-In default %v1%\n",
	"No infile specification, aborting\n",		/* 10 */
	"No file to process, aborting\n",			
	"Empty file, skipping\n", 
	"Deleting batch file: %v1%\n",
	"Execl",
	"Fork",						/* 15 */
	"Weird Response to Authorization: %v1%\n",	
	"Authorization Denied",
	"*** Unexpected response to command: %v1%\n%v2%\n",
	"Invalid argument: %v1%\n",
	"No Host Name Specified\n",			/* 20 */
	"No Batch file Specified\n",			
	"No prefix Supplied\n",
	"No args to use for filter\n",
	"No Error Log name provided\n",
	"rpost: Status Log",				/* 25 */
	"No Status Log name provided\n",
	"No Userid Specified\n",
	"No Password Specified\n",
	"No NNRP port Specified\n",
	"Invalid argument: %v1%, ignoring\n", /* 30 */
	"No language file specified\n",
	"No userid or password provided, unable to auto-authenticate",
	"No Perl script supplied, ignoring\n",
	"Perl set up- out of memory, ignoring\n",
	"Perl - error parse file %v1%, ignoring\n", /* 35 */
	"Perl - evaluation error %v1%, aborting\n",
	"Perl - invalid nr of return values, %v1%, aborting\n",
	"No rnews file name or rnews path provided\n",
	"Rnews file or path is invalid\n",
	"Uploading file-> %v1%\n", /*40*/
};

int nr_rpost_phrases = sizeof(default_rpost_phrases)/sizeof(default_rpost_phrases[0]);
