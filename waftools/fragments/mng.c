#include <libmng.h>

int main(int argc, char **argv)
{
    const char * p_ver = mng_version_text();
    return !p_ver || p_ver[0] == 0;
}
