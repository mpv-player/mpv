#include "config.h"
#include "core.h"
#include "osdep/io.h"
#include "osdep/terminal.h"

int wmain(int argc, wchar_t *argv[]);

// mpv does its own wildcard expansion in the option parser
int _dowildcard = 0;

int wmain(int argc, wchar_t *argv[])
{
    // If started from the console wrapper (see osdep/win32-console-wrapper.c),
    // attach to the console and set up the standard IO handles
    terminal_try_attach();

    char **argv_u8 = talloc_zero_array(NULL, char*, argc + 1);
    for (int i = 0; i < argc; i++)
        argv_u8[i] = mp_to_utf8(argv_u8, argv[i]);

    int ret = mpv_main(argc, argv_u8);

    talloc_free(argv_u8);
    return ret;
}
