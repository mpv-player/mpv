#!/usr/bin/perl

#
# (c) 2002-2004 by Florian Schilhabel <florian.schilhabel@web.de>
#
#
# version 0.1  initial release  22/08/2002
#
#
# If you have any comments, suggestions, etc., feel free to send me a mail ;-))
# flames and other things like that should go to /dev/null
# thankx to all the mplayer developers for this really *great* piece of software
#
#
# This script is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This script is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
#
#
#
use Getopt::Long;

# specify your default Settings here...

$abr_default = 128;          # The default AudioBitRate
$lang_default = "de";        # ... the language
$cdsize_default = 700;       # ... the CD-Rom Size
$writedev_default = "0,1,0"; # ... the CD Writer Device
$speed_default = 4;          # ... the writer speed
$dvd_device = "/dev/dvd";    # and the DVD Rom Device

# end of default Settings



sub delete_tempfiles {
	if (open(FILE, "< audio.stderr")) {
	close (FILE);
	system ("rm audio.stderr")
	}
	if (open(FILE, "< frameno.avi")) {
	close (FILE);
	system ("rm frameno.avi");
	}
	if (open(FILE, "< lavc_stats.txt")) {
	close (FILE);
	system ("rm lavc_stats.txt");
	}
}

GetOptions( 		"help" => \$help,
			"abr=i" => \$abr,
			"lang=s" =>\$lang,
			"cdsize=i" => \$cdsize,
			"dvd=i" => \$dvd_track,
			"keeptemp" => \$keeptemp,
			"shutdown" => \$shutdown,
			"out=s" => \$output,
			"writecd" => \$writecd,
			"writedev=s" => \$writedev,
			"speed=i" => \$speed,
			"dvd-device=s" => \$dvd_device );

if ($help) {
	print "Welcome to the DVD to DIVX Helper Script\n";
	print "\n";
	print "this script encodes a DVD track in 3-pass mode to libavcodec's mpeg4\n";
	print "Optionally it writes the resulting MovieFile to a CD-Rom\n";
	print "as well as the corresponding audio track to mp3\n";
	print "Optionally it writes the resulting MovieFile to a CD-Rom\n";
	print "and shuts down the Computer.\n";
	print "If you like, you can watch the mencoder output on /dev/tty8\n";
	print "Usage:\n";
	print "--help              show this text\n";
	print "--abr               (AudioBitRate) Please enter the desired bitrate\n";
	print "                    this can be either [96|128|192] kbit/sec.\n";
	print "                    Default: 128 kbit/sec.\n";
	print "--lang              specify the Language of the audio track\n";
	print "                    this can be for example <en> or <de>\n";
	print "                    Default: <de>\n";
	print "--dvd               specify the DVD Track, you want to encode\n";
	print "--cdsize            specify the Size of your CD-ROM\n";
	print "                    Default: 700MB\n";
	print "--shutdown          Shutdown the System, when the encoding process has finished\n";
	print "                    this will only be possible if you are root\n";
	print "--out               Specify the Name of your encoded Movie\n";
	print "                    The File Extension will be appended automatically\n";
	print "--writecd           takes the newly created Movie and writes it to a CD-Rom\n";
	print "--writedev          is the usual cdrecord device identifier\n";
	print "                    for example 0,1,0\n";
	print "--speed             the writing speed\n";
	print "                    Default: 4\n";
	print "--dvd-device        device to pull the video off\n";
	print "                    Default: /dev/dvd\n";
	exit;
}

delete_tempfiles();

# testing user values && set defaults...

if ($abr == 96){}
elsif ($abr == 128) {}
elsif ($abr == 192) {}
elsif ($abr == 0 ) {
	print "No AudioBitRate specified - Setting to Default Value\n";
	$abr = $abr_default;
}
else {
print "AudioBitRate not valid. please choose [96|128|192]\n";
exit;
}

if ($lang eq "") {
	print "No Language specified - Setting to Default Value\n";
	$lang = $lang_default;
}
else {}

if ($dvd_track == 0 ) {
	print "No DVD Track selected - You must specify one with: --dvd trackno.\n";
	exit;
}

if ($cdsize == 650) {}
elsif ($cdsize == 700) {}
elsif ($cdsize == 800) {}
elsif ($cdsize == 0 ) {
	print "No CD Size Specified - Setting to Default Value\n";
	$cdsize = $cdsize_default;
}

else	{
print "CD Size not valid. please choose [650|700|800]\n";
exit;
}

if ($output eq "") {
	print "No MovieName given - You must specify one with: --out [movie_name]\n";
	exit;
}
else {
	($name, $extension) = split(/./, $out);
	if ($extension eq "avi") {
		$vob_tempfile = "$name.vob";
		$avi_filename = "$output";
	}
	else {
		$vob_tempfile = "$output.vob";
		print "VOB CacheFile set to $vob_tempfile\n";
		$avi_filename = "$output.avi";
		print "Movie Filename set to $avi_filename\n";
	}
}

if ($shutdown) {
	# test who i am
	$user = `id -u`;
	if ($user == 0) {
		print "System will be shut down after Movie encoding\n";
	}
	else {
		print "Cannot shutdown the system after Movie encoding - you are not 'root'\n";
		exit;
	}
}

if ($writecd) {
	if ($writedev == "") {
		print "Setting CD Writer Device to Default Value\n";
		$writedev = $writedev_default;
	}
	if ($speed == 0) {
		print "Setting CD Writer Speed to Default value\n";
		$speed = $speed_default;
	}

}



###

print "Your Settings for this run are:\n";
print "AudioBitRate:    $abr\n";
print "Language:        $lang\n";
print "DVD-Track:       $dvd_track\n";
print "CD-Rom Size:     $cdsize\n";
print "Movie FIlename:  $avi_filename\n";
if ($writecd) {
	print "CD Writer Dev.:  $writedev\n";
	print "Writer Speed:    $speed\n";
}

# here comes the fun part...

print "precacheing...\n";
$status = system ("mencoder dvd://$dvd_track -ovc copy -oac copy -dvd-device $dvd_device -alang $lang -o $vob_tempfile 1>/dev/tty8 2>/dev/tty8");
die "Prechacheing failed. mencoder exited with Status Code $?" unless $status == 0;

print "Encoding Audio...\n";
$status = system ("mencoder $vob_tempfile -ovc frameno -oac mp3lame -lameopts br=$abr:cbr:vol=3 -o frameno.avi 1>./audio.stderr 2>/dev/tty8");
die "Encoding Audio failed. mencoder exited with Status Code $?" unless $status == 0;

# now we have to find out the recommended bitrate for the Video encoding process...
# my current method to find this out is, hmm, well, *strange*
# but anyway, it works ;-))

open(FILE, "< audio.stderr") or die "Unable to open audio.stderr.";
@lines = <FILE>;
foreach $line (@lines) {
	($index, $zz) = split(" ", $line);
	if ($index eq "Recommended") {
		($a, $b, $c, $d, $size, $f, $bitrate) = split(" ", $line);
		if ($cdsize == $size) {
			$video_bitrate = $bitrate;
			print "Setting Videobitrate to $video_bitrate\n";
		}
	}
}
close (FILE);

print "Encoding Video Stream, 1st pass...\n";
$status = system ("mencoder $vob_tempfile -ovc lavc -lavcopts vpass=1:vcodec=mpeg4:vbitrate=$video_bitrate:vhq -oac copy -o $avi_filename 1>/dev/tty8 2>/dev/tty8");
die "Encoding Video Stream failed. mencoder exited with Status Code $?" unless $status == 0;

print "Encoding Video Stream, 2nd pass...\n";
$status = system ("mencoder $vob_tempfile -ovc lavc -lavcopts vpass=2:vcodec=mpeg4:vbitrate=$video_bitrate:vhq -oac copy -o $avi_filename 1>/dev/tty8 2>/dev/tty8");
die "Encoding Video Stream failed. mencoder exited with Status Code $?" unless $status == 0;

print "finished encoding\n";


if ($writecd) {
	print "Now writing CD-Rom\n";
        $status = system("mkisofs -r -J $avi_filename | cdrecord  speed=$speed  dev=$writedev -data  - 2>/dev/tty8 1>/dev/tty8");
	die "Writing CD failed. cdrecord exited with Status Code $?" unless $status == 0;
}
delete_tempfiles();

print "Finished - have a nice day ;-)\n";
if ($shutdown) {
	system("halt");
	exit;
}
exit;
