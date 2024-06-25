#include "terminal.h"

#include "misc/bstr.h"

void terminal_init(void)
{
}

void terminal_setup_getch(struct input_ctx *ictx)
{
}

void terminal_uninit(void)
{
}

bool terminal_in_background(void)
{
    return false;
}

void terminal_get_size(int *w, int *h)
{
}

void terminal_get_size2(int *rows, int *cols, int *px_width, int *px_height)
{
}

PRINTF_ATTRIBUTE(2, 0)
int mp_console_vfprintf(void *wstream, const char *format, va_list args)
{
    return 0;
}

int mp_console_write(void *wstream, bstr str)
{
    return 0;
}

bool terminal_try_attach(void)
{
    return false;
}
