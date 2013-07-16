#include <X11/Xlib.h>
#include <X11/extensions/xf86vmode.h>

int main(int argc, char **argv)
{
    XF86VidModeQueryExtension(0, 0, 0);
    return 0;
}
