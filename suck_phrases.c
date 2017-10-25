const char *default_timer_phrases[] = {
	"Elapsed Time = %v1% mins %v2% seconds\n",		/* 0 */
	"%v1% Bytes received in %v2% mins %v3% secs, BPS = %v4%\n",
	"%v1% BPS",
	"%v1% Bytes received\n"
};

const char *default_chkh_phrases[] = {
	"Can not open %v1%, Skipping\n",		/* 0 */
	"Processing History File ",
	"Bogus line, ignoring: %v1%",
	"Processed history, %v1% dupes removed\n",
	"Out of Memory, skipping history check",
	"Processing History Database ",			/* 5 */
	"Unknown error reading history file %v1%\n",
};

const char *default_dedupe_phrases[] = {
	"Deduping ",				/* 0 */
	"Out of Memory, skipping Dedupe\n",
	"Deduped, %v1% items remaining, %v2% dupes removed.\n",
};

const char *default_killf_reasons[] = {
	"No Reason - major screwup",		/* 0 */
	"Over Hi-Line parameter",
	"Under Low-Line parameter",
	"Too many Newsgroups",
	"Not matched in Group Keep file",
	"Multiple Matches/Tiebreaker Used",     /* 5 */
	"Header match:",
	"Body match:",
	"Body Size exceeded, size=",
	"Body Size too small, size="
	"Too many Groups in Xref",             /* 10 */
};

const char *default_killf_phrases[] = {
	"Out of memory\n",				/* 0 */
	"Out of memory, can't use killfiles\n",
	"Invalid parameter line %v1%\n",
	"Unable to log killed article\n",
	"ARTICLE KILLED: group %v1% - %v2% - MsgID %v3%\n",
	"Out of memory, skipping killfile regex processing\n",		/* 5 */
	"No character to process for quote, ignoring\n",
	"Nothing to scan for in header scan, ignoring\n",
	"Out of memory, skipping killfile header scan processing\n",
	"Unable to write article to disk\n",
	"Nothing to scan for in body scan, ignoring\n", /* 10 */
	"Invalid regular expression, ignoring, %v1% - %v2%\n"
	"No character to process for non-regex, ignoring\n",
};

const char *default_killp_phrases[] = {
	"Out of memory processing killprg args",		/* 0 */
	"Setting up pipes",
	"Out of memory processing path",
	"%v1%: Unable to find, sorry",
	"Unable to write to child process",
	"Unable to read from child process",			/* 5 */
	"Process Error",
	"Child Process died",
	"Child process closed pipe, waiting for child to end\n",
	"Child exited with a status of %v1%\n",
	"Child exited due to signal %v1%\n",				/* 10 */
	"Child died, cause of death unknown\n",
	"killprg: Unable to log killed article",
	"killprg: ARTICLE KILLED: MsgID %v1%\n",
	"killperl: unable to allocate memory, ignoring\n",
	"killperl: error parsing program - %v1%\n", /* 15 */
	"killperl: evaluation error: %v1%\n",
	"killperl: invalid number of values returned: %v1%\n",
	"killperl: ARTICLE KILLED: MsgID %v1%\n",
	"killperl: Unable to log killed article",
};

const char *default_sucku_phrases[] = {
	"%v1%: Not a directory\n",		/* 0 */
	"%v1%: Write permission denied\n",
	"Lock File %v1%, Invalid PID, aborting\n",
	"Lock File %v1%, stale PID, removed\n",
	"Lock File %v1%, stale PID, Unable to remove, aborting\n",
	"Lock file %v1%, PID exists, aborting\n",	/* 5 */
	"Unable to create Lock File %v1%\n",
	"Error writing to file %v1%\n",
	"Error reading from file %v1%\n",
	"Invalid call to move_file(), notify programer\n"
};

const char *default_suck_phrases[] = {
	"suck: status log",		/* 0 */
	"Attempting to connect to %v1%\n",
	"Initiating restart\n",
	"No articles\n",
	"Closed connection to %v1%\n",
	"BPS",	/* 5*/
	"No local host name\n",
	"Cleaning up after myself\n",
	"Skipping Line: %v1%",
	"Invalid Line: %v1%",
	"Invalid number for maxread field: %v1% : ignoring\n",	/* 10 */
	"GROUP <%v1%> not found on host\n",
	"GROUP <%v1%>, unexpected response, %v2%\n",
	"%v1% - %v2%...High Article Nr is low, did host reset its counter?\n",
	"%v1% - limiting # of articles to %v2%\n",
	"%v1% - %v2% articles %v3%-%v4%\n",			/* 15 */
	"%v1% Articles to download\n",
	"Processing Supplemental List\n",
	"Supplemental Invalid Line: %v1%, ignoring\n",
	"Supplemental List Processed, %v1% articles added, %v2% articles to download\n",
	"Total articles to download: %v1%\n",		/* 20 */
	"Invalid Article Nr, skipping: %v1%\n",
	"Out of Memory, aborting\n",
	"Unable to write db record to disk, aborting\n",
	"Signal received, will finish downloading article.\n",
	"Notify programmer, problem with pause_signal()\n",	/* 25 */
	"\nGot Pause Signal, swapping pause values\n",
	"Weird Response to Authorization: %v1%",
	"Authorization Denied",
	"***Unexpected response to command, %v1%\n%v2%\n",
	"Moving newsrc to backup",				/* 30 */
	"Moving newrc to newsrc",
	"Removing Suck DB File",
	"Removing supplemental article file",
	"Invalid argument: %v1%\n",
	"No rnews batch file size provided\n",			/* 35 */
	"No postfix provided\n",
	"No directory name provided\n",
	"Invalid directory\n",
	"Invalid Batch arg: %v1%\n",
	"No Batch file name provided\n",			/* 40 */
	"No Error Log name provided\n",
	"No Status Log name provided\n",
	"No Userid provided\n",
	"No Passwd provided\n",
	"No Port Number provided\n",				/* 45 */
	"Invalid pause arguments\n",
	"No phrase file provided\n",
	"GROUP command not recognized, try the -M option\n",
	"No number of reconnects provided\n",
	"No host to post to provided\n", /* 50 */
	"No host name specified\n",
	"No timeout value specified\n",
	"No Article Number provided, can't use MsgNr mode\n",
	"Switching to MsgID mode\n",
	"Userid or Password not provided, unable to auto-authenticate\n", /* 55 */
	"No local active file provided\n",
	"Error writing article to file\n",
	"Can't pre-batch files, no batch mode specified\n",
	"Broken connection, aborting\n",
	"Restart: Skipping first article\n", /*60*/
	"No kill log file name provided\n",
	"No post filter provided\n",
	"-%v1%-%v2%-Message-ID too long, ignoring\n",
	"No history file path provided\n",
	"No default read for new active groups provided\n", /* 65 */
	"Invalid default read for new active group, must be <= 0\n",
	"Error setting up signal handlers",
	"Processing Nodownload File\n",
        "Invalid line in Nodownload File %v1%, ignoring\n",
        "Nodownload File processed, %v1% lines read, %v2% articles deleted, %v3% articles remaining\n", /* 70 */
	"%v1% - %v2%... High Number article is low, resetting last read to %v3%\n",
	"No batch_post count provide\n",
	"Userid or Password not provided, unable to authenticate\n",
	"No host name on command line, aborting\n",
};

const char *default_batch_phrases[] = {
	"Calling lmove\n",	/* 0 */
	"Forking lmove\n",
	"Running lmove\n",
	"Building INN Batch File\n",
	"Building RNews Batch File(s)\n",
	"Posting Messages to %v1%\n", /* 5 */
	"No batchfile to process\n",
	"Invalid batchfile line %v1%, ignoring",
	"Can't post: %v1% : reason : %v2%",
	"Error posting: %v1% : reason : %v2%",
	"%v1% Messages Posted\n", /* 10 */
	"Article Not Wanted, deleting: %v1% - %v2%",
	"Article Rejected, deleting: %v1% - %v2%",
};

const char *default_active_phrases[] = {
	"Unable to get active list from local host\n", /* 0 */
	"Out of memory reading local list\n",
	"Loading active file from %v1%\n",
	"Invalid group line in sucknewsrc, ignoring: %v1%\n",
	"Deleted group in sucknewsrc, ignoring: %v1%",
	"%v1% Articles to download\n", /* 5 */
	"No sucknewsrc to read, creating\n",
	"Out of memory reading ignore list, skipping\n",
	"Adding new groups from local active file to sucknewsrc\n",
	"Reading current sucknewsrc\n",
	"New Group - adding to sucknewsrc: %v1%\n", /* 10 */
	"Unable to open %v1%\n",
	"Active-ignore - error in expression %v1% - %v2%\n",
};

const char *default_xover_phrases[] = {
	"Unexpected XOVER command error, aborting: %v1%", /* 0 */
	"Xover - Invalid Article Number - %v1%",
	"Xover - Invalid Subject or early termination of line - %v1%",
	"Xover - Invalid Author or early termination of line - %v1%",
	"Xover - Invalid Date or early termination of line - %v1%",
	"Xover - Invalid Article-ID or early termination of line - %v1%", /* 5 */
	"Xover - Invalid Reference or early termination of line - %v1%",
	"Xover - Invalid Byte Count - %v1%",
	"Xover - Invalid Line Count - %v1%",
	"ARTICLE KILLED XOVER: group %v1% - %v2% - MsgID %v3%\n",
	"Xover=%v1%\n", /* 10 */
	"Unable to log Xover-killed article",
	"Out of memory, unable to process Xover killfiles",
	"No Message-ID in XOVER-%v1%- skipping\n",
};

const char *default_xover_reasons[] = {
	"Xover - Article too big", /* 0 */
	"Xover - Article too small",
	"Xover - Too many lines",
	"Xover - Too few lines",
	"Xover - Match on Header",
	"Xover - No Match on Group Keep file", /* 5 */
	"Xover - Multiple Matches/Tiebreaker used",
	"Xover - Too many Groups in Xref",
	"Xover - Match by external program",
	"Xover - Match by perl program"
};

int nr_suck_phrases= sizeof(default_suck_phrases)/sizeof(default_suck_phrases[0]);
int nr_timer_phrases= sizeof(default_timer_phrases)/sizeof(default_timer_phrases[0]);
int nr_chkh_phrases= sizeof(default_chkh_phrases)/sizeof(default_chkh_phrases[0]);
int nr_dedupe_phrases= sizeof(default_dedupe_phrases)/sizeof(default_dedupe_phrases[0]);
int nr_killf_reasons= sizeof(default_killf_reasons)/sizeof(default_killf_reasons[0]);
int nr_killf_phrases= sizeof(default_killf_phrases)/sizeof(default_killf_phrases[0]);
int nr_killp_phrases= sizeof(default_killp_phrases)/sizeof(default_killp_phrases[0]);
int nr_sucku_phrases= sizeof(default_sucku_phrases)/sizeof(default_sucku_phrases[0]);
int nr_active_phrases=sizeof(default_active_phrases)/sizeof(default_active_phrases[0]);
int nr_batch_phrases=sizeof(default_batch_phrases)/sizeof(default_batch_phrases[0]);
int nr_xover_phrases=sizeof(default_xover_phrases)/sizeof(default_xover_phrases[0]);
int nr_xover_reasons=sizeof(default_xover_reasons)/sizeof(default_xover_reasons[0]);

