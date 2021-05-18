#define _GNU_SOURCE
#include <sys/mman.h>
int main(int argc, char **argv) {
    memfd_create("mpv", MFD_CLOEXEC | MFD_ALLOW_SEALING);
    return 0;
}
