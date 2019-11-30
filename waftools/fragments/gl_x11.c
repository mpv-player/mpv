#include <X11/Xlib.h>
#include <GL/glx.h>
#include <stddef.h>

#ifndef GL_VERSION_2_0
#error "At least GL 2.0 headers needed."
#endif

int main(int argc, char *argv[]) {
  glXCreateContext(NULL, NULL, NULL, True);
  glXQueryExtensionsString(NULL, 0);
  glXGetProcAddressARB("");
  glXGetCurrentDisplay();
  return 0;
}
