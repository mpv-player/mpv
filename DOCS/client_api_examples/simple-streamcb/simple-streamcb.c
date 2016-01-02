// Build with: gcc -o simple-streamcb simple-streamcb.c `pkg-config --libs --cflags mpv`

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <mpv/client.h>
#include <mpv/stream_cb.h>

static void *open_fn(void *user_data, char *uri)
{
    FILE *fp = fopen((char *)user_data, "r");
    return fp;
}

static int64_t read_fn(void *cookie, char *buf, uint64_t nbytes)
{
    FILE *fp = (FILE *)cookie;
    size_t ret = fread(buf, 1, nbytes, fp);
    if (ret == 0) {
        return feof(fp) ? 0 : -1;
    }
    return ret;
}

static void close_fn(void *cookie)
{
    FILE *fp = (FILE *)cookie;
    fclose(fp);
}

static inline void check_error(int status)
{
    if (status < 0) {
        printf("mpv API error: %s\n", mpv_error_string(status));
        exit(1);
    }
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
    int val = 1;
    check_error(mpv_set_option(ctx, "osc", MPV_FORMAT_FLAG, &val));

    // Done setting up options.
    check_error(mpv_initialize(ctx));
    check_error(mpv_request_log_messages(ctx, "trace"));

    mpv_stream_cb_context *mpv_stream = mpv_get_sub_api(ctx, MPV_SUB_API_STREAM_CB);
    if (!mpv_stream) {
        printf("libmpv does not have the stream-cb sub-API.");
        exit(1);
    }
    mpv_stream_cb_init(mpv_stream, argv[1]);
    mpv_stream_cb_set_open_fn(mpv_stream, open_fn);
    mpv_stream_cb_set_read_fn(mpv_stream, read_fn);
    mpv_stream_cb_set_close_fn(mpv_stream, close_fn);

    // Play this file.
    const char *cmd[] = {"loadfile", "cb://fake", NULL};
    check_error(mpv_command(ctx, cmd));

    // Let it play, and wait until the user quits.
    while (1) {
        mpv_event *event = mpv_wait_event(ctx, 10000);
        if (event->event_id == MPV_EVENT_LOG_MESSAGE) {
            struct mpv_event_log_message *msg = (struct mpv_event_log_message *)event->data;
            printf("[%s] %s: %s", msg->prefix, msg->level, msg->text);
            continue;
        }
        printf("event: %s\n", mpv_event_name(event->event_id));
        if (event->event_id == MPV_EVENT_SHUTDOWN)
            break;
    }

    mpv_terminate_destroy(ctx);
    return 0;
}
