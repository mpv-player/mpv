#include "subprocess.h"

int mp_subprocess(char **args, struct mp_cancel *cancel, void *ctx,
                  subprocess_read_cb on_stdout, subprocess_read_cb on_stderr,
                  char **error)
{
    *error = "unsupported";
    return -1;
}
