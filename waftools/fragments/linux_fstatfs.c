#include <sys/vfs.h>
int main(int argc, char **argv) {
    struct statfs fs;
    fstatfs(0, &fs);
    fs.f_namelen;
    return 0;
}
