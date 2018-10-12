#include <windows.h>

#ifndef BASE_SEARCH_PATH_ENABLE_SAFE_SEARCHMODE
#define BASE_SEARCH_PATH_ENABLE_SAFE_SEARCHMODE (0x0001)
#endif

#include "config.h"

#include "common/common.h"
#include "osdep/io.h"
#include "osdep/terminal.h"
#include "osdep/main-fn.h"

#include "libmpv/client.h"

int wmain(int argc, wchar_t *argv[]);

// mpv does its own wildcard expansion in the option parser
int _dowildcard = 0;

static bool is_valid_handle(HANDLE h)
{
    return h != INVALID_HANDLE_VALUE && h != NULL &&
           GetFileType(h) != FILE_TYPE_UNKNOWN;
}

static bool has_redirected_stdio(void)
{
    return is_valid_handle(GetStdHandle(STD_INPUT_HANDLE)) ||
           is_valid_handle(GetStdHandle(STD_OUTPUT_HANDLE)) ||
           is_valid_handle(GetStdHandle(STD_ERROR_HANDLE));
}

static void microsoft_nonsense(void)
{
#if 0
    // stop Windows from showing all kinds of annoying error dialogs
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);

    // Enable heap corruption detection
    HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, NULL, 0);

    HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    WINBOOL (WINAPI *pSetSearchPathMode)(DWORD Flags) =
        (WINBOOL (WINAPI *)(DWORD))GetProcAddress(kernel32, "SetSearchPathMode");

    // Always use safe search paths for DLLs and other files, ie. never use the
    // current directory
    SetDllDirectoryW(L"");
    if (pSetSearchPathMode)
        pSetSearchPathMode(BASE_SEARCH_PATH_ENABLE_SAFE_SEARCHMODE);
#endif
}

#if 1
int wmain(int argc, wchar_t *argv[])
{
    microsoft_nonsense();

    // If started from the console wrapper (see osdep/win32-console-wrapper.c),
    // attach to the console and set up the standard IO handles
    bool has_console = terminal_try_attach();

    // If mpv is started from Explorer, the Run dialog or the Start Menu, it
    // will have no console and no standard IO handles. In this case, the user
    // is expecting mpv to show some UI, so enable the pseudo-GUI profile.
    bool gui = !has_console && !has_redirected_stdio();

    int argv_len = 0;
    char **argv_u8 = NULL;

    // Build mpv's UTF-8 argv, and add the pseudo-GUI profile if necessary
    if (argv[0])
        MP_TARRAY_APPEND(NULL, argv_u8, argv_len, mp_to_utf8(argv_u8, argv[0]));
    if (gui) {
        MP_TARRAY_APPEND(NULL, argv_u8, argv_len,
                         "--player-operation-mode=pseudo-gui");
    }
    for (int i = 1; i < argc; i++)
        MP_TARRAY_APPEND(NULL, argv_u8, argv_len, mp_to_utf8(argv_u8, argv[i]));
    MP_TARRAY_APPEND(NULL, argv_u8, argv_len, NULL);

    int ret = mpv_main(argv_len - 1, argv_u8);

    talloc_free(argv_u8);
    return ret;
}
#else
// Build with: gcc -o simple simple.c `pkg-config --libs --cflags mpv`

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <libmpv/client.h>
#include <libmpv/image.h>

static inline void check_error(int status)
{
	if (status < 0) {
		printf("mpv API error: %s\n", mpv_error_string(status));
		//exit(1);
	}
}


static void flip(image_t * frame)
{
	//unsigned short bpp = Data->Info.bmiHeader.biBitCount;
	unsigned int width = abs(frame->width);
	unsigned int height = abs(frame->height);
	unsigned int stride = abs(frame->stride);

	unsigned char* buffer = (unsigned char*)malloc(stride);

	unsigned char *cur = frame->buffer;

	for (int i = 0; i <= height / 2; i++)
	{
		memcpy(buffer, cur + i * stride, stride);
		memcpy(cur + i * stride, cur + (height - 1 - i)*stride, stride);
		memcpy(cur + (height - 1 - i)*stride, buffer, stride);
	}
	free(buffer);
}

void write_bmp(const char* filename, image_t *img)
{
	flip(img);
	int w = img->width;
	int h = img->height;
	int l = (w * 3 + 3) / 4 * 4;
	int bmi[] = { l*h + 54,0,54,40,w,h,1 | 3 * 8 << 16,0,l*h,0,0,100,0 };
	FILE *fp = fopen(filename, "wb");
	fprintf(fp, "BM");
	fwrite(&bmi, 52, 1, fp);
	fwrite(img->buffer, 1, l*h, fp);
	fclose(fp);
}

void image_cb_update_fn(image_t *cb_ctx)
{
	write_bmp("test_mpv.bmp", cb_ctx);
}

int main(int argc, char *argv[])
{
	if (argc != 2) {
		printf("pass a single media file as argument\n");
		return 1;
	}

	mpv_handle *ctx = mpv_create();
	if (!ctx) {
		printf("failed creating context\n");
		return 1;
	}

	// Enable default key bindings, so the user can actually interact with
	// the player (and e.g. close the window).
	check_error(mpv_set_option_string(ctx, "input-default-bindings", "yes"));
	mpv_set_option_string(ctx, "input-vo-keyboard", "yes");
	mpv_set_option_string(ctx, "vo", "image");
	int val = 1;
	//check_error(mpv_set_option(ctx, "osc", MPV_FORMAT_FLAG, &val));

	// Done setting up options.
	check_error(mpv_initialize(ctx));
	//
	mpv_image_cb_context * image_ctx = (mpv_image_cb_context *)mpv_get_sub_api(ctx, MPV_SUB_API_IMAGE_CB);
	mpv_image_set_update_callback(image_ctx, image_cb_update_fn, 0);
	// Play this file.
	const char *cmd[] = { "loadfile", argv[1], NULL };
	check_error(mpv_command(ctx, cmd));

	// Let it play, and wait until the user quits.
	while (1) {
		mpv_event *event = mpv_wait_event(ctx, 10000);
		printf("event: %s\n", mpv_event_name(event->event_id));
		if (event->event_id == MPV_EVENT_SHUTDOWN)
			break;
	}

	mpv_terminate_destroy(ctx);
	return 0;
}

#endif
