#include "gl_common.h"

/**
 * \brief adjusts the GL_UNPACK_ALGNMENT to fit the stride.
 * \param stride number of bytes per line for which alignment should fit.
 */
void glAdjustAlignment(int stride) {
  GLint gl_alignment;
  if (stride % 8 == 0)
    gl_alignment=8;
  else if (stride % 4 == 0)
    gl_alignment=4;
  else if (stride % 2 == 0)
    gl_alignment=2;
  else
    gl_alignment=1;
  glPixelStorei (GL_UNPACK_ALIGNMENT, gl_alignment);
}

