#include <sys/time.h>
#include <linux/videodev2.h>
int main(void)
{
    struct v4l2_ext_controls ext;
    return !!&ext.controls->value;
}
