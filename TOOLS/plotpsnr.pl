#!/usr/bin/perl
#
# Licence:          GPL
#
# 2003/02/09        Jonas Jermann <jjermann@gmx.net>
#
# Script:           Draw PSNR log graphs using gnuplot
#
# requires:         gnuplot

use Getopt::Long;

# MAIN
my %options;
my $dem_file="psnr.dem";

commandline();
demo_file();

system ("gnuplot $dem_file");
system("rm $dem_file $options{file}.[IPB] $options{file}.diff* 2>/dev/null");
exit 0;


# DEMO FILE
sub demo_file {

if ($options{file2}) {
    my @file1_cont = ();
    my @file2_cont = ();
    my $NewRow,$i,$j;

    open(LIST_IN, "$options{file}"); while( <LIST_IN> ) {
       $NewRow=[];
       @$NewRow = split(/[ ,]+/, $_);
       push( @file1_cont, $NewRow );
    }
    close(LIST_IN);

    open(LIST_IN2, "$options{file2}"); while( <LIST_IN2> ) {
       $NewRow=[];
       @$NewRow = split(/[ ,]+/, $_);
       push( @file2_cont, $NewRow );
    }
    close(LIST_IN2);

    open(LIST_OUT, ">$options{file}.diff");
    for($i=0; $i<=$#file2_cont; $i++) {
        print LIST_OUT " $file2_cont[$i]->[1],\ ";
        for($j=2; $j<=7; $j++) {
            $file2_cont[$i]->[$j] -= $file1_cont[$i]->[$j];
            print LIST_OUT " $file2_cont[$i]->[$j],\ ";
        }
        print LIST_OUT " $file2_cont[$i]->[8]\n";
    }
    close(LIST_OUT);
    $options{file}="$options{file}.diff";
}

if ($options{iframes}) { system("cat $options{file} | grep I > $options{file}.I"); }
if ($options{pframes}) { system("cat $options{file} | grep P > $options{file}.P"); }
if ($options{bframes}) { system("cat $options{file} | grep B > $options{file}.B"); }

open(DEM_FILE,">$dem_file");

print DEM_FILE "#PSNR Statistics
#---------------

set title \"PSNR Statistics\"
set data style fsteps
set xlabel \"Frames\"
set grid


";

if ($options{quant}) {
print DEM_FILE "# Quantizers
plot [] [0:] \\";
if ($options{pframes}) {
    print DEM_FILE "
    \"$options{file}.P\" using 1:2 t \"Quantizer: P frames\" w $options{qs}";
    if ($options{bframes} || $options{iframes}) { print DEM_FILE ",\\"; }
}
if ($options{bframes}) {
    print DEM_FILE "
    \"$options{file}.B\" using 1:2 t \"Quantizer: B frames\" w $options{qs}";
    if ($options{iframes}) { print DEM_FILE ",\\"; }
}
if ($options{iframes}) {
    print DEM_FILE "
    \"$options{file}.I\" using 1:2 t \"Quantizer: I frames\" w $options{qs}";
}
if (!($options{pframes} || $options{bframes} || $options{iframes})) {
    print DEM_FILE "
    \"$options{file}\" using 1:2 t \"Quantizer\" w $options{qs}";
}

print DEM_FILE "
pause -1

";
}


if ($options{size}) {
print DEM_FILE "# Frame size
plot \\";
if ($options{pframes}) {
    print DEM_FILE "
    \"$options{file}.P\" using 1:3 t \"Size: P frames\" w $options{ss}";
    if ($options{bframes}||$options{iframes}) { print DEM_FILE ",\\"; }
}
if ($options{bframes}) {
    print DEM_FILE "
    \"$options{file}.B\" using 1:3 t \"Size: B frames\" w $options{ss}";
    if ($options{iframes}) { print DEM_FILE ",\\"; }
}
if ($options{iframes}) {
    print DEM_FILE "
    \"$options{file}.I\" using 1:3 t \"Size: I frames\" w $options{ss}";
}
if (!($options{pframes}||$options{bframes}||$options{iframes})) {
    print DEM_FILE "
    \"$options{file}\" using 1:3 t \"Size\" w $options{ss}";
}

print DEM_FILE "
pause -1

";
}

if ($options{psnr}) {
print DEM_FILE "# PSNR
plot \\";
if ($options{pframes}) {
    print DEM_FILE "
    \"$options{file}.P\" using (\$1):(\$7) t \"PSNR (All): P frames\" w $options{ps}";
    if ($options{bframes}||$options{iframes}) { print DEM_FILE ",\\"; }
}
if ($options{bframes}) {
    print DEM_FILE "
    \"$options{file}.B\" using (\$1):(\$7) t \"PSNR (All): B frames\" w $options{ps}";
    if ($options{iframes}) { print DEM_FILE ",\\"; }
}
if ($options{iframes}) {
    print DEM_FILE "
    \"$options{file}.I\" using (\$1):(\$7) t \"PSNR (All): I frames\" w $options{ps}";
}
if (!($options{pframes}||$options{bframes}||$options{iframes})) {
    print DEM_FILE "
    \"$options{file}\" using (\$1):(\$7) t \"PSNR (All)\" w $options{ps}";
}

print DEM_FILE "
#\"$options{file}\" using (\$1):(\$4) t \"PSNR (Y)\" w $options{ps} \\
#\"$options{file}\" using (\$1):(\$5) t \"PSNR (Cb)\" w $options{ps} \\
#\"$options{file}\" using (\$1):(\$6) t \"PSNR (Cr)\" w $options{ps}
pause -1

";
}

print DEM_FILE "
reset";

close (DEM_FILE);
}



# USAGE
sub usage {
print STDERR <<EOF;

Usage: plotpsnr.pl [options] 'file'

Options:
  -h, --help	Display this help message
  -quant	Display quantizers
  -size		Display size
  -psnr		Display PSNR
  -iframes	Display I frames
  -pframes	Display P frames
  -bframes	Display B frames
  -aframes	Display all frames in different colors
  -cmp <file2>	Compare two files
  -qs <style>   Quantizer style
  -ss <style>   Size style
  -ps <style>   PSNR style

Default: -quant -size -psnr -qs "p" -ss "i" -ps "p"

Notes:
  Comparison is based on file2.
  Comparison assumes that the frame numbers of both files fit.

EOF
    exit 1;
}


# COMMAND LINE
sub commandline {
    $options{qs}="p";
    $options{ss}="i";
    $options{ps}="p";

    GetOptions(
        "help|h"	=> \&usage,
        "quant"		=> \$options{quant},
        "size"  	=> \$options{size},
        "psnr"		=> \$options{psnr},
	"cmp=s"		=> \$options{file2},
	"iframes"	=> \$options{iframes},
	"pframes"	=> \$options{pframes},
	"bframes"	=> \$options{bframes},
	"aframes"	=> sub { $options{iframes} = 1; 
				 $options{pframes}  = 1;
				 $options{bframes}  = 1; },
        "qs=s"     	=> \$options{qs},
        "ss=s"     	=> \$options{ss},
        "ps=s"     	=> \$options{ps},
    ) || usage();

if (!($options{quant}||$options{size}||$options{psnr})) {
    $options{quant}=1;
    $options{size}=1;
    $options{psnr}=1;
}

$options{file}="@ARGV";
}
