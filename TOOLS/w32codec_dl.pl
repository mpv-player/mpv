#!/usr/bin/perl

## usage: w32codec_dl.pl (codec.conf location)

# this script will use MS's codec dl interface as used by MS Media Player
# to attempt to locate the codecs listed in codecs.conf. It will download
# them to a directory "codecs/" below the current dir.
# you will need the libwww-perl stuff and the utility "cabextract"
# which may be found at http://www.kyz.uklinux.net/cabextract.php3

# By Tom Lees, 2002. I hereby place this script into the public domain.

#use LWP::Debug qw(+);
use LWP::UserAgent;

$ua = LWP::UserAgent->new;
$ua->agent ("Mozilla/4.0 (compatible; MSIE 5.0; Windows 98; DigExt)");

# Parse the etc/codecs.conf file
my $cconf = $ARGV[0];
open CCONF, "<$cconf";

my $codec = "(none)";
my $mscodec = 0;

my $cc, @ccl;

mkdir "codecs";
chdir "codecs";

CC: while (<CCONF>)
{
	next CC if (m/^[ \t]*\;/);
	s/\;.*//g;
	s/#.*//g;
	
	if (m/^videocodec (.*)/)
	{
		$codec = $1;
	}
	elsif (m/^[ \t]+driver (.*)/)
	{
		if ($1 eq "dshow" || $1 eq "vfw")
		{
			$mscodec = 1;
		}
		else
		{
			$mscodec = 0;
		}
	}
	elsif (m/^[ \t]+fourcc (.*)/ && $mscodec == 1)
	{
		$cclist = $1;
		chomp $cclist;
		#@ccl = ();
		do
		{
			if ($cclist =~ m/\"(....)\"[, ]*(.*)/)
			{
				$cc = $1;
				$cclist = $2;
			}
			elsif ($cclist =~ m/[, ]*(....)[, ]*(.*)/)
			{
				$cc = $1;
				$cclist = $2;
			}
			else
			{
				$cc = $cclist;
				$cclist = "";
			}
			if (!($cc =~ m/^[ \t]+/))
			{
				push @ccl, ($cc);
			}
		} while (length ($cclist) > 0);
	}
}
close CCONF;

# Find the codecs
open CODEC_CABS, ">codecs.locations.info";
%fcc_try = ();
while ($#ccl > 0)
{
	$cc = pop (@ccl);
	if (!$fcc_try{"$cc"})
	{
		$fcc_try{"$cc"} = 1;
		if (!find_codec ($cc))
		{
			print "$cc found\n";
		}
		else
		{
			print "MS didn't find $cc\n";
		}
	}
}
close CODEC_CABS;

%got_codecs = ();
sub find_codec
{
	my ($fourcc) = @_;
	
	my $guid = sprintf ("%08X", unpack ("V", $fourcc))."-0000-0010-8000-00AA00389B71";
	
	my $req = HTTP::Request->new (POST => "http://activex.microsoft.com/objects/ocget.dll");
	$req->header ('Accept', '*/*');
	$req->content_type ('application/x-www-form-urlencoded');
	$req->content ("CLSID=%7B${guid}%7D\n");
	#$req->content ('CLSID={'.${guid}.'}');
	
	my $res = $ua->request ($req);
	
	if ($res->is_success) {
		print "Lookup returned success... weird!\n";
		return 1;
	} else {
		# Codec location
		if ($res->code == 302)
		{
			my $loc = $res->headers->header ("Location");
			if (!$got_codecs{"$loc"})
			{
				print CODEC_CABS "$loc\n";
				$got_codecs{"$loc"} = 1;
				get_codec ($loc);
			}
#			else
#			{
#				print "Already have $loc\n";
#			}
			return 0;
		}
		else
		{
#			print "Lookup failed (Microsoft probably doesn't know this codec)\n";
			return 1;
		}
	}
}

sub get_codec
{
	my ($url) = @_;
	
	my $req = HTTP::Request->new (GET => $url);
	$req->header ("Accept", "*/*");
	my $res = $ua->request ($req);
	
	if ($res->is_success)
	{
		open TMP, ">tmp.cab" or die "Unable to open tmp.cab";
		print TMP $res->content;
		close TMP;
		
		system "cabextract tmp.cab";
		unlink "tmp.cab";
	}
	else
	{
		print "No such file!\n";
	}
}

