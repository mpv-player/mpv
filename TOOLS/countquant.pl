#!/usr/bin/perl -w

sub display_quants {
  $frames = 0;
  foreach $key (sort(keys(%quants))) {
    $frames += $quants{$key};
  }
  foreach $key (sort({ $a <=> $b } keys(%quants))) {
    printf("q=%d:\t% 6d, % 6.2f%%\n", $key, $quants{$key}, $quants{$key} *
           100 / $frames);
  }
  print("$lines lines processed, $frames frames found\n");
  printf("average quant. is: %f\n", $quant_total/$frames);
}

$lines = 0;
$thislines = 0;
$quant_total = 0;

while (<STDIN>) {
  $lines++;
  $thislines++;
  if (/ q:([0-9]+) /) {
    $quants{$1}++;
  } elsif (/ q:(([0-9]+)\.[0-9]+) /) {
    $quants{$2}++;
    $quant_total += $1;
  }
  if ((scalar(@ARGV) > 0) && ($thislines > $ARGV[0])) {
    display_quants();
    $thislines = 0;
  }
}

display_quants();


