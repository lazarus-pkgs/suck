#!/usr/bin/perl

# sample program for editting/changing the downloaded articles.
# this program is passed the directory where the articles are
# stored

# it modifies the Path: header, so you can add your own unique
# host to it, so you can tell INN not to upload these articles

opendir (DIRP, $ARGV[0]) or die "Can't open $ARGV[0]";

# get list of files, skipping hidden files
@files = grep ( !/^\./, readdir(DIRP));

foreach $file ( @files) {

	# read the file in
	$path  = "${ARGV[0]}/${file}";
	open FIP, "<${path}" or die "Can't read ${path}\n";
	@file = <FIP>;
	close FIP;

	# find the line and change it
	foreach $line ( @file) {
		if ( $line =~ /^Path: /) {
			$line =~ s/^Path: /Path: myhost\!/
		}
	}
	
	# save it back out
	open FIP, ">${path}" or die "Can't write to ${path}\n";
	print FIP @file;
	close FIP;
}

