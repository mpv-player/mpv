#! /usr/bin/env perl

use strict;
use warnings;

# Convert the contents of a file into a C string constant.
# Note that the compiler will implicitly add an extra 0 byte at the end
# of every string, so code using the string may need to remove that to get
# the exact contents of the original file.
# FIXME: why not a char array?

# treat only alphanumeric and not-" punctuation as safe
my $unsafe_chars = qr{[^][A-Za-z0-9!#%&'()*+,./:;<=>?^_{|}~ -]};

for my $file (@ARGV) {
    open my $fh, '<:raw', $file or next;
    print "/* Generated from $file */\n";
    while (<$fh>) {
        # replace unsafe chars with their equivalent octal escapes
        s/($unsafe_chars)/\\@{[sprintf '%03o', ord($1)]}/gos;
        print "\"$_\"\n"
    }
    close $fh;
}
