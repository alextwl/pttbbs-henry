#!/usr/bin/perl
# $Id: index.pl 2685 2005-03-29 16:25:29Z in2 $
use CGI qw/:standard/;
use lib qw/./;
use LocalVars;

sub main
{
    print redirect("/blog.pl/$1/")
	if( $ENV{REDIRECT_REQUEST_URI} =~ m|/\?(.*)| );

    return redirect("/blog.pl/$BLOGdefault/");
}

main();
1;

