#!/usr/bin/perl -w

# See README for more info
# This program uses two global lists
# @msgids - the message ids to be written
# @files  - index from message-id to file, so can match msgid to file

# things modifiable
$OUTPUT="suckothermsgs";
$MSGID_FOREGROUND="white";
$MSGID_BACKGROUND="blue";
$LISTBOX_WIDTH=30;		# width of both list boxes (file list and msgid list)
$ARTICLE_WIDTH=80;		# width of the article box

use Tk;

if ( $#ARGV != 0 ) {
    die "Usage: $0 path_to_headers <RETURN>\n";
}
$dir = $ARGV[0];

# first read in list of articles
opendir DIR, "$dir" or die "Can't open $dir\n";
@articles = sort(grep(/^\d+-\d+/,  readdir(DIR)));
closedir DIR;

$count = @articles;       # save nr of articles

if($count > 0) {
    $mw = MainWindow->new;
    $mw->title("Suck's Article Viewer");
 
    # set up our directory listing to select an article
    # the exportselection => 0 so both listboxes can have selections
    $frame=$mw->Frame()->pack(-side => "top");
    $f1=$frame->Frame()->pack(-side => "left"); # for our two listboxes
    $f1->Label(-text => "Article Listing")->pack(-side => "top");
    $listbox = $f1->Scrolled("Listbox", -scrollbars => "oe", -width => $LISTBOX_WIDTH, -exportselection => 0)->pack(-side => "top");
    $listbox->insert('end', @articles);
    $listbox->bind("<Button-1>", \&article_selected); # so can select a new file
    $listbox->selectionSet(0);          # so first item is selected and loaded

    # set up our listbox to show selected Message-IDs
    $f1->Label(-text => "Message-IDs Selected")->pack(-side => "top");
    $msgidbox = $f1->Scrolled("Listbox", -scrollbars => "oe", -width => $LISTBOX_WIDTH, -exportselection => 0)->pack(-side => "top");
    $msgidbox->bind("<Button-1>", \&msgid_selected); # so can go back to article

    #set up our article window
    $article = $frame->Scrolled("Text", -scrollbars => "osoe", -width => $ARTICLE_WIDTH, -wrap => "none")->pack(-side => "left");
    $article->tagConfigure('Msgid', -foreground => $MSGID_FOREGROUND, -background => $MSGID_BACKGROUND); 
    # so we can highlight message id 
    
    &article_selected(); # this is here so that it can load the initial article, can't do this until article window is set up

    # set up our button window
    $bw=$mw->Frame()->pack(-side => "bottom"); # button window
    $bw->Button(-text => "Select Article", -command => \&select_msgid)->pack(-side => "left");
    $bw->Button(-text => "Next", -command => \&next_one)->pack(-side => "left");
    $bw->Button(-text => "Previous", -command => \&previous)->pack(-side => "left");
    $bw->Button(-text => "De-Select Article", -command => \&deselect)->pack(-side => "left");
    $bw->Button(-text => "Done", -command => \&save_msgids)->pack(-side => "left");
    MainLoop;
}
else { 
    print "No articles found\n";
}
#--------------------------------------------------------------------------------------
sub article_selected() {
    # this routine displays the article selected into our article window
    my ( $msgid, @list, $file, $path, @spl, $found, $count);

    $msgid = 0;

    @list = $listbox->curselection(); # get the index of the current item selected
    $file = $listbox->get($list[0]);

    # clear out the old file
    $article->configure(-state => "normal");  # so we can insert 
    $article->delete("1.0", "end");
    # now load the file
    $path = "$dir/$file";
    open FP, "<$path" or die "Can't read $path\n";
    while (<FP>) {
	# find the Message-ID and highlight it
	if (( $msgid == 0)  && ( $_ =~ /^Message-ID:/) ) { 
	    @spl = split(/[<>]/, $_, 3);                       #split to get only  the actual message id
	    $article->insert('end', $spl[0]);
	    $article->insert('end', "<${spl[1]}>", 'Msgid');   # tag ONLY the messageid, we need to add back the <> split took off
	    $article->insert('end', $spl[2]);
	    $msgid = 1;             # so we only get Message-ID and not any extraneous stuff (like quoted articles)
	}
	else {
	    $article->insert('end', $_);
	}
    }
    close FP;
    $article->configure(-state => "disabled"); # so user can't edit

    # now, see if we have a Message-ID for it, and if so, select it
    # @files is our index of files to msgid, go thru it and see if
    # our listbox index is in it
    $found = -1;
    if(defined @files) {
	$count = -1; # keep track of where we are in msgid list
	for( $count = 0 ; $count <= $#files ; $count++) {
	    if($files[$count] == $list[0]) {
		$found = $count;
		last;
	    }
	}
	# clear the current list 
	@list = $msgidbox->curselection();
	if(defined $list[0]) {
	    $msgidbox->selectionClear($list[0]);
	}
	# show it
	if($found >= 0) {
	    $msgidbox->selectionSet($found);
	    $msgidbox->see($found);
	}
    }
}
#--------------------------------------------------------------------------------------
sub next_one() {

    my ( @list, $itemnr);

    # increment the listbox, and load it
    @list = $listbox->curselection(); 
    $itemnr = $list[0] + 1;
    if( $itemnr < ($count - 1) ) {
	# the -1 because index start at 0
	$listbox->selectionClear($itemnr - 1); 
	$listbox->selectionSet($itemnr);
	$listbox->see($itemnr);
	&article_selected();
    }
}
#--------------------------------------------------------------------------------------
sub previous() {
    my ( @list, $itemnr) ;

    # decrement the listbox and load it
    @list = $listbox->curselection();
    $itemnr = $list[0] - 1;
    if( $itemnr >= 0 ) {
	$listbox->selectionClear($itemnr + 1);
	$listbox->selectionSet($itemnr);
	$listbox->see($itemnr);
	&article_selected();
    }
}
#---------------------------------------------------------------------------------------
sub select_msgid() {
    # get the tagged Msgid from the text and add it to the listbox
    my ( @index, $msgid, $count, $file, @list, $tmp);
    
    @index = $article->tagRanges("Msgid");
    $msgid = $article->get($index[0], $index[1]);   # get only the first one, just in case more than one got selected 
    
    # make sure we don't already have it, can't use grep cause of the wacky chars in the msgid 
    $count = 0;
    foreach $tmp ( @msgids ) {
	if ( $tmp eq $msgid ) {
	    $count++;
	}
    }

    if($count == 0 ) {
	@list = $listbox->curselection(); # get the index of the current items selected
	push @files,  $list[0];     # so if user clicks on a msg id, we can go to that article
	push @msgids, $msgid;    # add to our list for later saving and making sure we don't already have it
	$msgidbox->insert('end', $msgid);
	$msgidbox->selectionSet('end');
	$msgidbox->see('end');
    } 
}
#---------------------------------------------------------------------------------------
sub msgid_selected() {
    # selected a msgid, put that article back in the viewer
    my (@list, @l2);

    @list = $msgidbox->curselection();
    @l2 = $listbox->curselection();
   
    $listbox->selectionClear($l2[0]);  # clear the current selection
    # @files is a list of indexes to files for the msgids, use it to go back to file
    $listbox->selectionSet($files[$list[0]]);   # make this one the current selectio
    $listbox->see($files[$list[0]]);
    &article_selected();  # so it gets redisplayed
}
#----------------------------------------------------------------------------------------
sub deselect() {
    # if the current message id is selected, remove it from the list

    my @list;

    @list = $msgidbox->curselection();   # if its selected, it'll be here
    if(defined $list[0]) {
	$msgidbox->delete($list[0]); # remove from listbox
	splice @msgids, $list[0], 1; # remove it from list of msgids
	splice @files,  $list[0], 1; # remove it from list of file indexes
    }
}
#--------------------------------------------------------------------------------------------
sub save_msgids() {
    # save the @msgids list to OUTPUT
    
   # pop up the filename query
    if( ! Exists($popup)) {
	$outfile = $OUTPUT;
	$popup = $mw->Toplevel();
	$popup->title( "Save Message-IDs?");
	$popup->Label(-text => "File Path")->pack(-side => "top");
	$popup->Entry(-textvariable => \$outfile, -width => 50)->pack(-side => "top");
	$buttons = $popup->Frame()->pack(-side => "top");
	$buttons->Button(-text => "Yes", -command => \&save_file)->pack(-side => "left");
	$buttons->Button(-text => "Cancel", -command => sub { $popup->destroy } )->pack(-side => "left");
	$buttons->Button(-text => "No", -command => sub { exit })->pack(-side => "left"); 
    }
}
#---------------------------------------------------------------------------------------------
sub save_file() {
    # save our output file
    my ( $item ) ;

    if(defined @msgids) {
	if ( open FPTR, ">$outfile" ) {
	    foreach $item ( @msgids ) {
		print FPTR "${item}\n";
	    }
	    close FPTR;
	    exit;
	}
	else {
	    errmsg("Unable to create $outfile");
	}
    }
}
#-----------------------------------------------------------------------------------------------
sub errmsg() {
    
    # any error message
    if ( ! Exists($errp)) {
	$errp = $mw->Toplevel();
	$errp->Label(-text => $_[0])->pack(-side => "top");
	$errp->Button(-text => "Okay", -command => sub { $errp->destroy })->pack(-side => "top");
    }
}
