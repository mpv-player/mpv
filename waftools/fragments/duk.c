#include "../../duktape/duktape.h"

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;
#if (DUK_VERSION < 10000) || (DUK_VERSION >= 20000)
    MPV_Unsupported_DUK_VERSION;
#endif
    return 0;
}
