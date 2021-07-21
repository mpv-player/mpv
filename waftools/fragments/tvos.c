#include <TargetConditionals.h>
#include <assert.h>
int main (int argc, char **argv) {
    static_assert(TARGET_OS_TV, "TARGET_OS_TV defined to zero!");
    return 0;
}
