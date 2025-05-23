#include "osdep/mac/app_bridge.h"

// Cocoa absolutely requires creating the NSApplication singleton and running it on the main thread.
int main(int argc, char *argv[])
{
    return cocoa_main(argc, argv);
}
