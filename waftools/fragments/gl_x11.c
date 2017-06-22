#include <X11/Xlib.h>
#include <GL/glx.h>
#include <stddef.h>

int main(int argc, char *argv[]) {
  glXCreateContext(NULL, NULL, NULL, True);
  glXQueryExtensionsString(NULL, 0);
  glXGetProcAddressARB("");
  glXGetCurrentDisplay();
  return 0;
}
