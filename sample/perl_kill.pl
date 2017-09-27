# sample perl routine for killfiles
package Embed::Persistant;

sub perl_kill {

	my $header = $_[0];  # header
	my $retval = 0;

	if($header =~ /WANTED/) {
		$retval = 1; 
	}
	return $retval;
}
