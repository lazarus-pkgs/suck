# sample perl routine for rpost

# 3 key things
# 1. we get in the input file name
# 2. we return the output file name
# 3. You MUST have the close on both the input
#    file and output file.  Remember this is 
# running as a subroutine, not a full blown program
# and perl only closes files upon program exit, or
# reuse of file handle.

# WARNING USING /tmp/tmp.rpost is insecure,
# you should change this to somewhere normal
# users can't read or write.
#
package Embed::Persistant;

sub perl_rpost {

	my $infile = $_[0];  # input file name
	my $outfile = "/tmp/tmp.rpost";

	# open files for input and output

	# if we can't open the file, fail with the filename
	# rpost will then handle the error
	# We do this since rpost can't differentiate between
	# a missing file or a script file
	open IFP, "<${infile}" or return $infile;
#	open IFP, "<${infile}" or die "Can't read ${infile}";
	open OFP, ">${outfile}" or die "Can't create ${outfile}";

	# now strip out what we don't need
	while ( $line = <IFP>) {
	    if($line !~ /^NNTP-Posting|^Xref|^X-Trace|^X-Comp/) {
		# write out to file
		print OFP $line;
	    }
	    if($line =~ /^$/) {
		last;	# end of header don't need to test anymore
		# this test is here so the line gets printed out.
	    }
	}
	while ( $line = <IFP> ) {
		print OFP $line;
	}

	close IFP;
	close OFP;

	# return output file name
	return $outfile;
}
