# sample perl routine for killfiles
package Embed::Persistant;

sub perl_overview {

    my $overview = $_[0];

    do_debug("got $overview");
}

sub perl_xover {

	my $header = $_[0];  # header
	my $retval = 0;

	if($header =~ /WANTED/) {
		$retval = 1; 
	}
	do_debug("got $header");
	return $retval;
}
sub do_debug {
	
	open FIP, ">>debug.child";
	print FIP @_;
	close FIP;
}
