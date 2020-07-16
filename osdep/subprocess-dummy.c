#include "subprocess.h"

void mp_subprocess2(struct mp_subprocess_opts *opts,
                    struct mp_subprocess_result *res)
{
    *res = (struct mp_subprocess_result){.error = MP_SUBPROCESS_EUNSUPPORTED};
}
