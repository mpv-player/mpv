#!/usr/bin/perl -w
use strict;
use constant CMD => "./fastmem2-k7";

sub dobench {
	my $i;
	my ($runs, $sleep, $command) = @_;
	for($i = 0; $i < $runs; $i++) {
		sleep $sleep;
		system $command;
	}
}

print "Single run of sse bench with 1sec sleep:\n";
&dobench(1,1,CMD);
print "Sleeping 10seconds before starting next bench!\n";
sleep 10;		
print "10 runs of sse bench with 0sec sleep:\n";
&dobench(10,0,CMD);
print "Sleeping 10seconds before starting next bench!\n";
sleep 10;		
print "10 runs of sse bench with 1sec sleep:\n";
&dobench(10,1,CMD);
print "Sleeping 10seconds before starting next bench!\n";
sleep 10;		
print "10 runs of sse bench with 2sec sleep:\n";
&dobench(10,2,CMD);
print "Sleeping 10seconds before starting next bench!\n";
sleep 10;		
print "10 runs of sse bench with 3sec sleep:\n";
&dobench(10,3,CMD);
print "Bench finished!\n";		
