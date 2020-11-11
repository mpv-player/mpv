#include "terminal.h"

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

void mp_write_console_ansi(void *wstream, char *buf)
{
}

bool terminal_try_attach(void)
{
    return false;
}
