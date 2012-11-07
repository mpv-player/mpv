use 5.008;
use strict;
use warnings;

# ABSTRACT: Module collection to parse Matroska files.
package Parse::Matroska;

=head1 DESCRIPTION

C<use>s L<Parse::Matroska::Reader>. See the documentation
of the modules mentioned in L</"SEE ALSO"> for more information
in how to use this module.

It's intended for this module to contain high-level interfaces
to the other modules in the distribution.

=head1 SOURCE CODE

L<https://github.com/Kovensky/Parse-Matroska>

=head1 SEE ALSO

L<Parse::Matroska::Reader>, L<Parse::Matroska::Element>,
L<Parse::Matroska::Definitions>.

=cut

use Parse::Matroska::Reader;

1;
