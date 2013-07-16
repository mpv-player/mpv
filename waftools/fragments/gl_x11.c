#include <X11/Xlib.h>
#include <GL/glx.h>
#include <GL/gl.h>

int main(int argc, char *argv[]) {
  glXCreateContext(NULL, NULL, NULL, True);
  glFinish();
  return 0;
}
