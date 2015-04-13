#include "config.h"
#include "core.h"

#if HAVE_COCOA_APPLICATION
#include "osdep/macosx_application.h"
#endif

int main(int argc, char *argv[])
{
#if HAVE_COCOA_APPLICATION
    return cocoa_main(mpv_main, argc, argv);
#else
    return mpv_main(argc, argv);
#endif
}
