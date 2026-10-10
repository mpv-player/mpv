#include "terminal.h"

#include "misc/bstr.h"
#include "osdep/threads.h"

static mp_static_mutex output_lock = MP_STATIC_MUTEX_INITIALIZER;

void terminal_lock_output(void)
{
    mp_mutex_lock(&output_lock);
}

void terminal_unlock_output(void)
{
    mp_mutex_unlock(&output_lock);
}

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

MP_PRINTF_ATTRIBUTE(2, 0)
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

void terminal_set_mouse_input(bool enable)
{
}

bool terminal_set_resize_callback(void (*cb)(void *ctx), void *ctx)
{
    return false;
}
