#!/usr/bin/perl -w
use strict;
# Filename: mp.pl
# Date    : created 2001-07-24
# Author  : Felix Buenemann <atmosfear at users.sourceforge.net>
# Idea by : David Chan <prometheus at theendofthetunnel.org>
# License : GNU General Public License (GPL)
#           (refer to: http://www.fsf.org/licenses/gpl.txt)
#
# Description:
# Small Perl helper script that allows to play multiple files with MPlayer.
# Wildcards are supported (eg. "mp.pl -vo x11 /data/*.avi").
#
# Configuration:
# If MPlayer is not in your path, give the full
# path to mplayer binary in the line below.
# (example: "/usr/local/bin/mplayer")
use constant MPLAYER => "mplayer";

my (@parms, @files);

die
"mp.pl: No parameters given!

MPlayer multifile playback helper script 0.9
Copyleft 2001 by Felix Buenemann

Syntax: mp.pl <parameters> <files>

Where <parameters> are all possible commandline switches for mplayer and
<files> can be either a list of files, like file1 file2 file3 and/or a
wildcard definition, like *.avi.

Example: mp.pl -vo x11 /dvd/VIDEO_TS/VTS_05_*.VOB movie.asf
\n"
if ($#ARGV < 0) || ($ARGV[0] =~ m/^--*(h|help)/);

foreach (@ARGV) {
	if(m/^-\w+/) { push @parms, $_ }
	elsif(-f $_ && -r _ && -B _) { push @files, $_ }
	else { push @parms, $_ }
}
die "No valid files to process!\n" unless @files;
foreach (@files) {
	print "Invoking MPlayer for '$_'...\n";
	system(MPLAYER, @parms, $_)
		or die "Couldn't execute MPlayer: $!\n";
	($? >> 8) != 1
		and die "Couldn't properly execute MPlayer, aborting!\n";
}
# EOF
