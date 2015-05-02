#include "osdep/macosx_application.h"

// This is needed because Cocoa absolutely requires creating the NSApplication
// singleton and running it in the "main" thread. It is apparently not
// possible to do this on a separate thread at all. It is not known how
// Apple managed this colossal fuckup.
int main(int argc, char *argv[])
{
    return cocoa_main(argc, argv);
}
