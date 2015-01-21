#include <X11/Xlib.h>
#include <GL/glx.h>
#include <GL/gl.h>
#include <stddef.h>

int main(int argc, char *argv[]) {
  glXCreateContext(NULL, NULL, NULL, True);
  glXQueryExtensionsString(NULL, 0);
  glXGetProcAddressARB("");
  glXGetCurrentDisplay();
  glFinish();
  (void)GL_RGB32F;          // arbitrary OpenGL 3.0 symbol
  (void)GL_LUMINANCE16;     // arbitrary OpenGL legacy-only symbol
  return 0;
}
