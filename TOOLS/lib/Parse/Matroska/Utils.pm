use strict;
use warnings;

# ABSTRACT: internally-used helper functions
package Parse::Matroska::Utils;

use Exporter;
our @ISA       = qw{Exporter};
our @EXPORT_OK = qw{uniq uncamelize};

=method uniq(@array)

The same as L<List::MoreUtils/"uniq LIST">.
Included to avoid depending on it since it's
not a core module.

=cut
sub uniq(@) {
  my %seen;
  return grep { !$seen{$_}++ } @_;
}

=method uncamelize($string)

Converts a "StringLikeTHIS" into a
"string_like_this".

=cut
sub uncamelize($) {
    local $_ = shift;
    # lc followed by UC: lc_UC
    s/(?<=[a-z])([A-Z])/_\L$1/g;
    # UC followed by two lc: _UClclc
    s/([A-Z])(?=[a-z]{2})/_\L$1/g;
    # strip leading _ that the second regexp might add; lowercase all
    s/^_//; lc
}
