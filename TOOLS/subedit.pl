#!/usr/bin/perl -w

# A script for pipelined editing of subtitle files.
# Copyright (C) 2004 Michael Klepikov <mike72@mail.ru>
#
# Version 1.0  initial release  28-Mar-04
#
# Comments, suggestions -- send me an mail, but the recommended way is
# to enhance/fix on your own and submit to the distribution;)
# If you like, I can review the fixes.
#
# This script is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
# Retain original credits when modifying.
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

use Math::BigInt;

# Constants
my $FMT_UNKNOWN = 0;
my $FMT_SRT = 1;

# Argument values
my $DEBUG = 0;
my $inFormat;
my $outFormat;
my $shiftMilli;
my $scaleMilli;
my $splitFromMilli;
my $splitToMilli;

## Process command line
while (defined ($argVal = shift)) {
  if ($argVal eq "-d" || $argVal eq "--debug") {
    $DEBUG = 1;
  } elsif ($argVal eq "-if" || $argVal eq "--input-format") {
    $inFormat = shift;
    usage ("Must specify input format") if ! $inFormat;
    if ($inFormat =~ /^srt/i) {
      $inFormat = $FMT_SRT;
    } else {
      usage ("Invalid input format");
    }
  } elsif ($argVal eq "-of" || $argVal eq "--output-format") {
    $outFormat = shift;
    usage ("Must specify input format") if ! $outFormat;
    if ($outFormat =~ /^srt/i) {
      $outFormat = $FMT_SRT;
    } else {
      usage ("Invalid output format");
    }
  } elsif ($argVal eq "-s" || $argVal eq "--shift") {
    my $argTime = shift;
    if (! defined $argTime ||
	! defined ($shiftMilli = getTimeMillis ($argTime))) {
      usage ("Invalid shift time value");
    }
  } elsif ($argVal eq "-c" || $argVal eq "--scale") {
    my $argTime = shift;
    if (! defined $argTime ||
	! defined ($scaleMilli = getTimeMillis ($argTime))) {
      usage ("Invalid scale time value");
    }
  } elsif ($argVal eq "-f" || $argVal eq "--split-from") {
    my $argTime = shift;
    if (! defined $argTime ||
	! defined ($splitFromMilli = getTimeMillis ($argTime))) {
      usage ("Invalid split start time value");
    }
  } elsif ($argVal eq "-t" || $argVal eq "--split-to") {
    my $argTime = shift;
    if (! defined $argTime ||
	! defined ($splitToMilli = getTimeMillis ($argTime))) {
      usage ("Invalid split end time value");
    }
  } elsif ($argVal eq "-h" || $argVal eq "--help") {
    usage ();
  } else {
    usage ("Unrecognized argument $argVal");
  }
}

# Input format defaults to SRT
$inFormat = $FMT_SRT if (! defined $inFormat);
# Output format defaults to the same as input
$outFormat = $inFormat if (! defined $outFormat);

## Read

my $subs;
if ($inFormat == $FMT_SRT) {
  $subs = readSRT (*STDIN);
  printf STDERR ("Read %d SRT subs\n", scalar @{$subs}) if $DEBUG;
  # Sort by start time
  @{$subs} = sort {$a -> {srtStartTime} <=> $b -> {srtEndTime}} @{$subs};
}

## Transform

if (defined $shiftMilli && 0 != $shiftMilli) {
  printf STDERR ("Shift: %d milliseconds\n", $shiftMilli) if $DEBUG;
  shiftSRT ($subs, $shiftMilli);
}

if (defined $splitFromMilli || defined $splitToMilli) {
  if ($DEBUG) {
    my $printFrom = (defined $splitFromMilli) ? $splitFromMilli : "-";
    my $printTo = (defined $splitToMilli) ? $splitToMilli : "-";
    printf STDERR ("Split: from $printFrom to $printTo\n");
  }
  splitSRT ($subs, $splitFromMilli, $splitToMilli);
}

if (defined $scaleMilli && 0 != $scaleMilli) {
  my $lastSubIdx = scalar @{$subs} - 1;
  if ($lastSubIdx >= 0) {
    my $lastTimeOrig = $subs -> [$lastSubIdx] -> {srtEndTime};
    if ($lastTimeOrig == 0) {
      die "Cannot scale when last subtitle ends at 00:00:00,000";
    }
    my $lastTimeScaled = $lastTimeOrig + $scaleMilli;
    printf STDERR ("Scale: %d/%d\n", $lastTimeScaled, $lastTimeOrig) if $DEBUG;
    scaleSRT ($subs, $lastTimeScaled, $lastTimeOrig);
  }
}

## Write
if ($outFormat == $FMT_SRT) {
  writeSRT (*STDOUT, $subs);
}

# Close STDOUT, as recommended by Perl manual
# (allows diagnostics on disc overflow, etc.)
close (STDOUT) || die "Cannot close output stream: $!";

exit 0;

## Subroutines

# Convert string time format to milliseconds
# SRT style: "01:20:03.251", and "," is allowed instead of "."
# Return undef in case of format error
sub getTimeMillis
{
  $_ = shift;
  my $millis = 0;

  if (/\s*(.*)[\.,]([0-9]+)?\s*$/) { # Fraction; strip surrounding spaces
    #print STDERR "frac: \$1=$1 \$2=$2\n" if $DEBUG;
    $_ = $1;
    $millis += ("0." . $2) * 1000 if $2;
  }
  if (/(.*?)([0-9]+)$/) { # Seconds
    #print STDERR "secs: \$1=$1 \$2=$2\n" if $DEBUG;
    $_ = $1;
    $millis += $2 * 1000 if $2;
  }
  if (/(.*?)([0-9]+):$/) { # Minutes
    #print STDERR "mins: \$1=$1 \$2=$2\n" if $DEBUG;
    $_ = $1;
    $millis += $2 * 60000 if $2;
  }
  if (/(.*?)([0-9]+):$/) { # Hours
    #print STDERR "mins: \$1=$1 \$2=$2\n" if $DEBUG;
    $_ = $1;
    $millis += $2 * 3600000 if $2;
  }
  if (/(.*?)\-$/) { # Minus sign
    $_ = $1;
    $millis *= -1;
  }
  $millis = undef if (! /^$/); # Make sure we ate everything up
  if ($DEBUG) {
    if (defined $millis) {
      #print STDERR "time value match: $millis ms\n";
    } else {
      #print STDERR "time mismatch\n";
    }
  }
  return $millis;
}

# Convert milliseconds to SRT formatted string
sub getTimeSRT
{
  my $t = shift;
  my $tMinus = "";
  if ($t < 0) {
    $t = -$t;
    $tMinus = "-";
  }
  my $tMilli = $t % 1000;
  $t /= 1000;
  my $tSec = $t % 60;
  $t /= 60;
  my $tMin = $t % 60;
  $t /= 60;
  my $tHr = $t;
  return sprintf ("%s%02d:%02d:%02d,%03d",
		  $tMinus, $tHr, $tMin, $tSec, $tMilli);
}

# Read SRT subtitles
sub readSRT
{
  local *IN = shift;
  my $subs = [];

  $_ = <IN>;
  print STDERR "Undefined first line\n" if ! defined $_ && $DEBUG;
  my $lineNo = 1;
  READ_SUBS:
  while (defined $_) {
    # Each loop iteration reads one subtitle from <IN>
    my $sub = {};

    # print STDERR "Reading line $lineNo\n" if $DEBUG;

    # Skip empty lines
    while (/^\s*$/) {
      last READ_SUBS if ! ($_ = <IN>);
      ++$lineNo;
    }

    # Subtitle number
    if (/^\s*([0-9]+)\s*$/) {
      $sub -> {srtNumber} = $1;
      # print "SRT num: $1\n" if $DEBUG;
    } else {
      die "Invalid SRT format at line $lineNo";
    }

    # Timing
    if ($_ = <IN>) {
      ++$lineNo;
    } else {
      die "Unexpected end of SRT stream at line $lineNo";
    }
    # print STDERR "LINE: $_\n" if $DEBUG;
    if (/^\s*(\S+)\s*--\>\s*(\S+)\s*$/) {
      my $startMillis = getTimeMillis ($1);
      my $endMillis = getTimeMillis ($2);
      die "Invalid SRT timing format at line $lineNo: $_"
	if ! defined $startMillis || ! defined $endMillis;
      $sub -> {srtStartTime} = $startMillis;
      $sub -> {srtEndTime} = $endMillis;
    } else {
      die "Invalid SRT timing format at line $lineNo: $_";
    }

    # Text lines
    my $subLines = [];
    while (1) {
      last if ! ($_ = <IN>); # EOF ends subtitle
      ++$lineNo;
      last if /^\s*$/; # Empty line ends subtitle
      ($_ = $_) =~ s/\s+$//; # Strip trailing spaces
      push @{$subLines}, $_;
    }
    die "No text in SRT subtitle at line $lineNo" if 0 == scalar @{$subLines};
    $sub -> {lines} = $subLines;

    # Append subtitle to the list
    push @{$subs}, $sub;
  }
  print STDERR "SRT read ok, $lineNo lines\n" if $DEBUG;

  return $subs;
}

# Write SRT subtitles
sub writeSRT
{
  use integer; # For integer division
  local *OUT = shift;
  my $subs = shift;

  my $subNum = 0;
  foreach (@{$subs}) {
    ++$subNum;

    my $sub = $_;
    my $sTimeSRT = getTimeSRT ($sub -> {srtStartTime});
    my $eTimeSRT = getTimeSRT ($sub -> {srtEndTime});
    printf OUT ("%d\n%s --> %s\n", $subNum, $sTimeSRT, $eTimeSRT);
    foreach (@{$sub -> {lines}}) {
      printf OUT ("%s\n", $_);
    }
    printf OUT "\n";
  }
  printf STDERR ("Wrote %d SRT subs\n", $subNum) if $DEBUG;
}

# Shift SRT subtitles by a given number of seconds.
# The number may be negative and fractional.
sub shiftSRT
{
  use integer; # $shiftMilli could be passed as float
  my $subs = shift;
  my $shiftMilli = shift;

  foreach (@{$subs}) {
    $_ -> {srtStartTime} += $shiftMilli;
    $_ -> {srtEndTime} += $shiftMilli;
  }
}

# Multiply each subtitle timing by a divident and divide by divisor.
# The idea is that the divident is usually the new total number of
# milliseconds in the subtitle file, and the divisor is the old
# total number of milliseconds in the subtitle file.
# We could simply use a double precision real coefficient instead of
# integer divident and divisor, and that could be good enough, but
# using integer arithmetics *guarantees* precision up to the last
# digit, so why settle for good enough when we can have a guarantee.
#
# Uses Math::BigInt arithmetics, because it works with numbers
# up to (total number of milliseconds for a subtitle timing)^2,
# which could be on the order of approximately 1e+13, which is
# larger than maximum 32-bit integer.
# There is a performance loss when using BigInt vs. regular floating
# point arithmetics, but the actual performance is quite acceptable
# on files with a few thousand subtitles.
sub scaleSRT
{
  use integer; # Divident and divisor could be passed as floats, truncate
  my $subs = shift;
  my $scaleDividend = shift;
  my $scaleDivisor = shift;

  foreach (@{$subs}) {
    my $ss = Math::BigInt -> new ($_ -> {srtStartTime});
    $ss = $ss -> bmul ($scaleDividend);
    $_ -> {srtStartTime} = $ss -> bdiv ($scaleDivisor) -> bsstr ();
    my $se = Math::BigInt -> new ($_ -> {srtEndTime});
    $se = $se -> bmul ($scaleDividend);
    $_ -> {srtEndTime} = $se -> bdiv ($scaleDivisor) -> bsstr ();
  }
}

# Extract a fragment within a given time interval
# Either "from" or "to" may be undefined
sub splitSRT
{
  use integer; # fromMilli and toMilli could be passed as floats, truncate
  my $subs = shift;
  my $fromMilli = shift;
  my $toMilli = shift;

  my $iSub = 0;
  while ($iSub < scalar @{$subs}) {
    $_ = $subs -> [$iSub];
    my $keep = 0;
    if (! defined $fromMilli || $_ -> {srtEndTime} >= $fromMilli) {
      # The subtitle ends later than the start boundary

      # Fix overlapping start timing,
      # but only of the start boundary is not infinite (undef)
      if (defined $fromMilli && $_ -> {srtStartTime} < $fromMilli) {
	$_ -> {srtStartTime} = $fromMilli;
      }
      if (! defined $toMilli || $_ -> {srtStartTime} <= $toMilli) {
	# The subtitle begins earlier than the end boundary

	# Fix overlapping end timing,
	# but only of the end boundary is not infinite (undef)
	if (defined $toMilli && $_ -> {srtEndTime} > $toMilli) {
	  $_ -> {srtEndTime} = $toMilli;
	}

	# All conditions met, all fixes done
	$keep = 1;
      }
    }
    if ($keep) {
      ++$iSub;
    } else {
      splice @{$subs}, $iSub, 1;
    }
  }
}

# Print brief usage help
# Accepts an optional error message, e.g. for errors parsing command line
sub usage
{
  my $msg = shift;
  my $exitCode = 0;

  if (defined $msg) {
    $exitCode = 2;
    print STDERR "$msg\n";
  }

  print STDERR <<USAGE;
Usage: $0 [switches]
  -if,--input-format <fmt>  input format; supported: SRT
                            default is SRT
  -of,--output-format <fmt> output format; supported: SRT
                            default is same as input format
  -s,--shift <time>         shift all subtitles by <time>
                            (format: [-]hh:mm:ss,fraction)
  -c,--scale <time>         scale by adding <time> to overall duration
  -f,--split-from <time>    Drop subtitles that end before <time>
  -t,--split-to <time>      Drop subtitles that start after <time>
                            (will truncate timing if it overlaps a boundary)
  -r,--renumber             renumber SRT subtitles in output
  -d,--debug                enable debug output
  -h,--help                 this help message

All times could be negative. Input/output may also contain negative timings,
which is sometimes useful for intermediate results.
SRT subtitles are always renumbered on output.

EXAMPLES

Split subtitle file into two disks at a boundary of one hour 15 minutes:

  subedit.pl --split-to 1:15:0 < all.srt > p1.srt
  subedit.pl -f 1:15:0 < all.srt | subedit.pl --shift -1:15:0 > p2.srt

Join the previous two disks back into one file:

  subedit.pl -s 1:15:00 < p2.srt | cat p1.srt - | subedit.pl > all.srt

Correct a situation where the first subtitle starts in sync with the video,
but the last one starts 3.5 seconds earlier than the speech in the video,
assuming the first subtitle timing is 00:01:05.030:

  subedit.pl -s -1:5.03 | subedit.pl -c 3.5 | subedit.pl -s 1:5.03
USAGE

  exit $exitCode;
}
