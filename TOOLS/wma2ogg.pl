#!/usr/bin/perl

#################################
# wma converter 0.3.6 for Linux #
#################################

# Made by Peter Simon <simon.peter@linuxuser.hu>
# License: GNU GPL
#
# Needed programs:
#
# Getopt::Long - Perl Module
# MPlayer - http://www.mplayerhq.hu
# BladeEnc - http://bladeenc.mp3.no
# oggenc - http://www.vorbis.com
# mp3info - http://www.ibiblio.org/mp3info

# changelog:
# 0.1.0
# decoding all files to wav without any switches and encoders
# 0.2.0
# converting to mp3 or ogg with bitrate, delete options
# 0.2.1
# L.A.M.E. support for fanatics
# Default output filetype: ogg
# Some error warnings
# 0.3.0
# Reading the wma tags out and puting into the mp3 or the ogg tags. (What the MPlayer shows of those.)
# Default output bitrate: the original bitrate
# Simlink for the default encode mode (wma2ogg = ogg, wma2mp3 = mp3)
# 0.3.1
# Neglecting missing encoders if those are not in use.
# 0.3.2
# Using mp3info for the mp3 tags
# 0.3.3
# Convert more then one files in the start dir.
# 0.3.4-5
# Some bugfixes.
# 0.3.6
# Some bugfixes by Diego Biurrun

# Why BladeEnc?
# Eg.: The L.A.M.E.'s code isn't compatible for some hardwer-decoders. Makes noise and clashings.
# I never met this trouble using BladeEnc.
# That's it.

use Getopt::Long qw(:config pass_through);

$ver="0.3.6";

GetOptions("help|?",\&showhelp, 'a' => \$all, "file|f=s" => \@files,"t=s" => \$mtype, "lame" => \$needlame, "del" => \$delete, "br=i" => \$sbrate);

if (@ARGV) {
	foreach (@ARGV) {
			error ("Missing parameter after the $_ switch!");
	}
	exit;
}

print "wma2ogg $ver\nPeter Simon <simon.peter\@linuxuser.hu>";

if (!$all && !@files) {
	error ("There is no selected file!");
	exit;
}

if ($0 =~/wma2mp3/ && !$mtype) {
	$mtype="mp3";
}

if ($mtype eq "ogg") {
	$ttype{"ogg"}="oggenc";
	needed_prgs (oggenc, "http://www.vorbis.com");
}

if ($needlame && $mtype eq "ogg") {
	error ("\nYou can not use L.A.M.E. & oggenc together!\n");
	exit;
}

if (!$mtype && !$needlame) {
	$mtype="ogg";
	$ttype{"ogg"}="oggenc";
	needed_prgs (oggenc, "http://www.vorbis.com");
}


if (!$needlame && $mtype eq "mp3") {
	$ttype{"mp3"}="BladeEnc";
	needed_prgs (BladeEnc, "http://bladeenc.mp3.no", mp3info, "http://www.ibiblio.org/mp3info");
}

if ($needlame) {
	$mtype="mp3";
	$ttype{"mp3"}="lame";
	needed_prgs (lame, "http://lame.sourceforge.net", mp3info, "http://www.ibiblio.org/mp3info");
}

# Main program
	print "\nUsing lame - WARNING - this is not optimal!\n";
	ch_needed ();
	ch_type ();
	ch_files ();
	decode();
# the end.


sub ch_type {
	$o_type=$ttype{$mtype};

	if ($mtype ne "wav") {
		$def_path=$ENV{PATH};
		@exec_path=split /\:/, $def_path;
		foreach $temp_path (@exec_path) {
			if (-d $temp_path && !$enc_ok) {
				$enc_ok=`find $temp_path -name $o_type -type f -maxdepth 1 2>/dev/null`;
				chomp ($enc_ok);
			}
		}
	}
	if ((!$o_type || !$enc_ok) && $mtype ne "wav") {
		error("Unknown encoder.");
		exit;
	}
}

sub ch_br {
	if ($sbrate && ((($sbrate <32 || $sbrate>350) && $mtype eq "mp3")  ||   (($sbrate<64 || $sbrate>500) && $mtype eq "ogg") )) {
		error("Invalid, missing or redundant bitrate.");
		exit;
	}
}

sub ch_files {
	if ($all && @files) {
		error ("You can't use -a switch and -f together!");
		exit;
	}

	if ($all) {
		@enc_files=`ls | grep '.wma'\$`;
		foreach (@enc_files) {
			chomp $_;
		}
	}

	if (@files) {
		@enc_files=@files;
	}

}

sub showhelp {
print "\n\nUsage: wma2ogg [OPTIONS] (-f FILE1 | -f FILE2 ... | -a)\n
-f, -file         filename
-a                converts all wma files in the current directory\n
OPTIONS:
-t                output filetype (ogg, mp3) [default=ogg]
-lame             I wanna use L.A.M.E. sure enough!
-br               bitrate (kb/s) [default=from the wma]
-del              remove wma file(s) after the transcoding\n";
print $miss_text;
print "\n$errtext\nExiting program.\n";
}

sub error {
	$errtext=@_[0];
	showhelp ();

}

sub missing_prg {
	$what=$keys;
	$that=$needed{$keys};
	$miss_text.="\nThe needed \'$what\' program is missing, see: $that!";
}

sub ch_needed {

	`perl -e 'use Getopt::Long;' 2>./err`;
	open (FILE, "<./err");
	while ($sor=<FILE>) {
		if ($sor =~ /Can\'t locate/) {
			missing_prg ("Getopt::Long", ": your Perl is too old... (uhhh... get a new one!)");
		}
	}
	`rm ./err`;


	foreach $keys (keys %needed) {
		`$keys 2>./err`;
		open (FILE, "<./err");
		while ($sor=<FILE>) {
			if ($sor =~ /$keys\: command not found/) {
				missing_prg ();
				$error=1;
			}
		}
	`rm ./err`;
	}

	close FILE;
	if ($error) {
	showhelp ();
	exit;
	}
}

sub get_tags {
	my $outfile;
	open (FILE, "<./1");
	while ($sor=<FILE>) {
		$outfile.=$sor;
	}
	close FILE;

	$outfile=~ s/\((\d+\,\d+)\ kbit\)/$1/e;
	print "\noriginal bitrate: $1";
	$kept_orig_brate=$1;
	$kept_orig_brate=~ s/(\d+)/$1/e;
	$kept_orig_brate=$1;

	if (!$sbrate) {
		$brate=$kept_orig_brate;
		print " (kept as default)";
	} else {
		$brate=$sbrate;
		print " (new: $brate,0)";
	}

	ch_br ();

	`rm ./1`;
	my @temp_info=split /Clip\ info\:/, $outfile;
	my @temp2_info=split /\n/, @temp_info[1];
	my @temp_title=split /\ /, $temp2_info[1],3;
	my @temp_author=split /\ /, $temp2_info[2],3;
	my @temp_copyright=split /\ /, $temp2_info[3],3;
	my @temp_comments=split /\ /, $temp2_info[4],3;
	return ($temp_title[2], $temp_author[2], $temp_copyright[2], $temp2_comments[2]);
}

sub needed_prgs {
	%needed=(
		$_[0]=>$_[1],
		$_[2]=>$_[3],
		mplayer=>"http://www.mplayerhq.hu",
	);
}

sub mp3_info {
			if ($title) {
				$infofile=" -t '$title'";
			}
			if ($author) {
				$infofile.=" -a '$author'";
			}
			if ($comments) {
				$infofile.=" -c '$comments'";
			}

			`mp3info "$p_name.$mtype" $infofile`;

			undef ($infofile);
}


sub decode {
	foreach (@enc_files) {
		$wav_name=$_;
		$wav_name=~ s/(.+)\./$1/e;
		$p_name=$1;
		$wav_name=$p_name.".wav";
		$pwd=`pwd`;
		chomp $pwd;
		$pwd.="/t2";
		print "\nConverting $_ to \"wav\" file.\n";
		print "Using MPlayer...\n";

		`mplayer "$_" -ao pcm -input conf="$pwd" 2>/dev/null >./1`;
		`mv "audiodump.wav" "$wav_name"`;

		@tags=get_tags ();

		$title=$tags[0];
		print "\ntitle: $title";
		$author=$tags[1];
		print "\nauthor: $author";
		$copyright=$tags[2];
		print "\ncopyright: $copyright";
		$comments=$tags[3];
		print "\ncomments: $comments";

		$comments=$copyright." ".$comments."Transcoded by wma2ogg";

		print "\n\nConverting $wav_name to \"$mtype\" file.";

		print "\nUsing $o_type...";

		if ($mtype eq "ogg") {
			$br_sw="b";
			if ($title) {
				$infofile=" -t '$title'";
			}
			if ($author) {
				$infofile.=" -a '$author'";
			}
			if ($comments) {
				$infofile.=" -c COMMENT='$comments'";
			}

			`"$o_type" "-$br_sw" "$brate" "-Q" "./$wav_name" $infofile "-o" "$p_name.$mtype"`;
			`rm -f "$wav_name"`;
		}
		if ($mtype eq "mp3" && !$needlame) {
			$br_sw="br";
			`"$o_type" "$wav_name" "$p_name.$mtype" "-$br_sw" "$brate" 2>/dev/null >/dev/null`;

			mp3_info ();

			`rm -f "$wav_name"`;
		}
		if ($mtype eq "mp3" && $needlame) {

			$br_sw="b";
			`"$o_type" "-$br_sw" "$brate" "-f" "$wav_name" "$p_name.$mtype" 2>/dev/null >/dev/null`;

			mp3_info ();

			`rm -f "$wav_name"`;
		}
		if ($delete) {
			`rm -f "$_"`;
		}
		if (-e "./err") {
			`rm ./err`;
		}
	}
	print "\n\nDone.\n\n";
}

