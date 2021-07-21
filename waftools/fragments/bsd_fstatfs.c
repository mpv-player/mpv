#include <sys/mount.h>
#include <sys/param.h>
int main(int argc, char **argv) {
    struct statfs fs;
    fstatfs(0, &fs);
    fs.f_fstypename;
    return 0;
}
