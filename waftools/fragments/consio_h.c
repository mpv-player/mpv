#include <sys/consio.h>
#include <sys/ioctl.h>
int main(int argc, char **argv) {
    int m;
    ioctl(0, VT_GETMODE, &m);
    return 0;
}
