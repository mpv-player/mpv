#include <X11/Xlib.h>
#include <GL/glx.h>
#include <GL/gl.h>
#include <stddef.h>

int main(int argc, char *argv[]) {
  glXCreateContext(NULL, NULL, NULL, True);
  glXQueryExtensionsString(NULL, 0);
  glXGetProcAddressARB("");
  glFinish();
  return 0;
}
