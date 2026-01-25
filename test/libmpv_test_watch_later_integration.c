/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Integration test for watch-later subtitle persistence.
 *
 * This test verifies that external subtitles loaded via sub-add command
 * (simulating drag-and-drop) are properly saved to watch-later config
 * and restored when the video is reopened.
 *
 * Test flow:
 * 1. Load a video file
 * 2. Add an external subtitle using sub-add command
 * 3. Write watch-later config (quit-watch-later)
 * 4. Verify the config file contains the subtitle path
 * 5. Create a new mpv instance and load the same video
 * 6. Verify the subtitle track is automatically loaded
 */

#include "libmpv_common.h"

#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>

static char temp_dir[512];
static char video_path[512];
static char sub_path[512];
static char watch_later_dir[512];

// Find the watch-later config file for our video (skip redirect entries)
static int find_watch_later_file(char *out_path, size_t out_size)
{
    DIR *dir = opendir(watch_later_dir);
    if (!dir)
        return -1;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.')
            continue;
        snprintf(out_path, out_size, "%s/%s", watch_later_dir, entry->d_name);

        // Skip redirect entries - we want the actual config file
        FILE *f = fopen(out_path, "r");
        if (f) {
            char line[256];
            if (fgets(line, sizeof(line), f)) {
                fclose(f);
                // Skip if this is a redirect entry
                if (strstr(line, "# redirect entry"))
                    continue;
                // Found a real config file
                closedir(dir);
                return 0;
            }
            fclose(f);
        }
    }
    closedir(dir);
    return -1;
}

// Check if a file contains a specific string
static int file_contains_string(const char *filepath, const char *needle)
{
    FILE *f = fopen(filepath, "r");
    if (!f)
        return 0;

    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, needle)) {
            fclose(f);
            return 1;
        }
    }
    fclose(f);
    return 0;
}

// Count external subtitle tracks
static int count_external_sub_tracks(void)
{
    int64_t track_count = 0;
    int ret = mpv_get_property(ctx, "track-list/count", MPV_FORMAT_INT64, &track_count);
    if (ret < 0)
        return 0;

    int external_sub_count = 0;
    for (int i = 0; i < track_count; i++) {
        char prop[64];

        // Check type
        snprintf(prop, sizeof(prop), "track-list/%d/type", i);
        char *type = NULL;
        if (mpv_get_property(ctx, prop, MPV_FORMAT_STRING, &type) < 0)
            continue;
        int is_sub = strcmp(type, "sub") == 0;
        mpv_free(type);
        if (!is_sub)
            continue;

        // Check if external
        snprintf(prop, sizeof(prop), "track-list/%d/external", i);
        int is_external = 0;
        mpv_get_property(ctx, prop, MPV_FORMAT_FLAG, &is_external);

        if (is_external)
            external_sub_count++;
    }
    return external_sub_count;
}

static void cleanup_temp_dir(void)
{
    // Remove watch-later files
    DIR *dir = opendir(watch_later_dir);
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_name[0] == '.')
                continue;
            char path[1024];
            snprintf(path, sizeof(path), "%s/%s", watch_later_dir, entry->d_name);
            unlink(path);
        }
        closedir(dir);
    }
    rmdir(watch_later_dir);

    // Remove test files
    unlink(video_path);
    unlink(sub_path);
    rmdir(temp_dir);
}

static void setup_test_files(const char *sample_video)
{
    // Create temp directory
    snprintf(temp_dir, sizeof(temp_dir), "/tmp/mpv_test_%d", getpid());
    mkdir(temp_dir, 0755);

    // Setup paths
    snprintf(video_path, sizeof(video_path), "%s/test_video.mkv", temp_dir);
    snprintf(sub_path, sizeof(sub_path), "%s/test_sub.srt", temp_dir);
    snprintf(watch_later_dir, sizeof(watch_later_dir), "%s/watch_later", temp_dir);
    mkdir(watch_later_dir, 0755);

    // Copy sample video to temp location
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "cp '%s' '%s'", sample_video, video_path);
    if (system(cmd) != 0)
        fail("Failed to copy sample video\n");

    // Create a simple SRT subtitle file
    FILE *f = fopen(sub_path, "w");
    if (!f)
        fail("Failed to create subtitle file\n");
    fprintf(f, "1\n00:00:00,000 --> 00:00:01,000\nTest subtitle line 1\n\n");
    fprintf(f, "2\n00:00:01,000 --> 00:00:02,000\nTest subtitle line 2\n\n");
    fclose(f);
}

/*
 * Test Phase 1: Load video, add subtitle, save watch-later config
 */
static void test_phase1_save_subtitle(void)
{
    printf("Phase 1: Load video and add external subtitle\n");

    // Configure mpv to use our temp watch-later directory
    char opt[1024];
    snprintf(opt, sizeof(opt), "%s", watch_later_dir);
    int ret = mpv_set_option_string(ctx, "watch-later-directory", opt);
    if (ret < 0)
        printf("Warning: Could not set watch-later-directory: %s\n", mpv_error_string(ret));

    // Enable save-position-on-quit
    int flag = 1;
    set_option_or_property("save-position-on-quit", MPV_FORMAT_FLAG, &flag, true);

    // Verify sub-files is in watch-later-options
    char *wl_opts = NULL;
    get_property("watch-later-options", MPV_FORMAT_STRING, &wl_opts);
    if (!strstr(wl_opts, "sub-files"))
        fail("sub-files not in watch-later-options!\n");
    printf("  watch-later-options includes sub-files: OK\n");
    mpv_free(wl_opts);

    // Load the video
    printf("  Loading video: %s\n", video_path);
    reload_file(video_path);

    // Wait for file to load - check for either FILE_LOADED or playback start
    int loaded = 0;
    for (int i = 0; i < 100 && !loaded; i++) {
        mpv_event *ev = mpv_wait_event(ctx, 0.1);
        if (ev->event_id == MPV_EVENT_FILE_LOADED ||
            ev->event_id == MPV_EVENT_PLAYBACK_RESTART)
            loaded = 1;
    }

    // Also check if a video track exists (file is loaded even if events missed)
    int64_t track_count = 0;
    if (!loaded) {
        mpv_get_property(ctx, "track-list/count", MPV_FORMAT_INT64, &track_count);
        if (track_count > 0)
            loaded = 1;
    }

    if (!loaded)
        fail("Video file did not load\n");
    printf("  Video loaded: OK\n");

    // Verify no external subtitles yet
    int ext_subs = count_external_sub_tracks();
    printf("  External subtitle tracks before sub-add: %d\n", ext_subs);

    // Add external subtitle using sub-add (simulates drag-and-drop)
    printf("  Adding external subtitle via sub-add: %s\n", sub_path);
    const char *cmd[] = {"sub-add", sub_path, NULL};
    command(cmd);

    // Give it a moment to load
    for (int i = 0; i < 10; i++)
        mpv_wait_event(ctx, 0.05);

    // Verify subtitle was added
    ext_subs = count_external_sub_tracks();
    printf("  External subtitle tracks after sub-add: %d\n", ext_subs);
    if (ext_subs < 1)
        fail("External subtitle was not added!\n");
    printf("  Subtitle added: OK\n");

    // Save watch-later config by using quit-watch-later command
    printf("  Saving watch-later config...\n");

    // First verify that we're in a valid playback state
    char *path = NULL;
    int ret2 = mpv_get_property(ctx, "path", MPV_FORMAT_STRING, &path);
    printf("  Current path: %s (ret=%d)\n", path ? path : "NULL", ret2);
    if (path)
        mpv_free(path);

    // Check track count before quitting
    int64_t track_count2 = 0;
    mpv_get_property(ctx, "track-list/count", MPV_FORMAT_INT64, &track_count2);
    printf("  Track count before quit: %lld\n", (long long)track_count2);

    const char *quit_cmd[] = {"quit-watch-later", NULL};
    command(quit_cmd);

    // Wait for shutdown
    while (mpv_wait_event(ctx, 1)->event_id != MPV_EVENT_SHUTDOWN) {}
    printf("  Watch-later saved: OK\n");
}

/*
 * Test Phase 2: Verify watch-later config file contains subtitle
 */
static void test_phase2_verify_config(void)
{
    printf("Phase 2: Verify watch-later config file\n");

    // List all files in the watch-later directory
    printf("  Files in watch-later directory:\n");
    DIR *dir = opendir(watch_later_dir);
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_name[0] == '.')
                continue;
            char filepath[1024];
            snprintf(filepath, sizeof(filepath), "%s/%s", watch_later_dir, entry->d_name);
            printf("    %s:\n", entry->d_name);
            FILE *f = fopen(filepath, "r");
            if (f) {
                char line[256];
                while (fgets(line, sizeof(line), f)) {
                    printf("      %s", line);
                }
                fclose(f);
            }
        }
        closedir(dir);
    }

    char config_path[1024];
    if (find_watch_later_file(config_path, sizeof(config_path)) < 0)
        fail("No watch-later config file found in %s\n", watch_later_dir);

    printf("  Using config file: %s\n", config_path);

    // Print the config file contents for debugging
    printf("  Config file contents:\n");
    FILE *f = fopen(config_path, "r");
    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            printf("    %s", line);
        }
        fclose(f);
    }
    printf("\n");

    // Check if file contains sub-files-append with our subtitle path
    if (!file_contains_string(config_path, "sub-files-append="))
        fail("Config file does not contain sub-files-append entry\n");

    printf("  Config contains sub-files-append: OK\n");

    // Verify it contains our subtitle filename
    if (!file_contains_string(config_path, "test_sub.srt"))
        fail("Config file does not contain our subtitle filename\n");

    printf("  Config contains subtitle path: OK\n");
}

/*
 * Test Phase 3: Create new mpv instance, load video, verify subtitle restores
 */
static void test_phase3_restore_subtitle(void)
{
    printf("Phase 3: Verify subtitle restores on reopen\n");

    // Create new mpv instance
    ctx = mpv_create();
    if (!ctx)
        fail("Failed to create new mpv instance\n");

    // Configure the new instance
    char opt[1024];
    snprintf(opt, sizeof(opt), "%s", watch_later_dir);
    mpv_set_option_string(ctx, "watch-later-directory", opt);
    mpv_set_option_string(ctx, "vo", "null");
    mpv_set_option_string(ctx, "ao", "null");

    int flag = 1;
    mpv_set_option(ctx, "resume-playback", MPV_FORMAT_FLAG, &flag);

    if (mpv_initialize(ctx) < 0)
        fail("Failed to initialize mpv\n");

    // Load the same video
    printf("  Reopening video: %s\n", video_path);
    const char *cmd[] = {"loadfile", video_path, NULL};
    mpv_command(ctx, cmd);

    // Wait for file to load
    int loaded = 0;
    for (int i = 0; i < 50 && !loaded; i++) {
        mpv_event *ev = mpv_wait_event(ctx, 0.1);
        if (ev->event_id == MPV_EVENT_FILE_LOADED)
            loaded = 1;
    }
    if (!loaded)
        fail("Video file did not load on second open\n");
    printf("  Video loaded: OK\n");

    // Give tracks time to load
    for (int i = 0; i < 20; i++)
        mpv_wait_event(ctx, 0.05);

    // Verify external subtitle was restored
    int ext_subs = count_external_sub_tracks();
    printf("  External subtitle tracks after restore: %d\n", ext_subs);

    if (ext_subs < 1)
        fail("External subtitle was NOT restored! Fix did not work.\n");

    printf("  Subtitle restored: OK\n");
    printf("\n*** Integration test PASSED! ***\n");
    printf("The fix correctly saves and restores dynamically-loaded subtitles.\n");
}

int main(int argc, char *argv[])
{
    if (argc < 2)
        fail("Usage: %s <sample_video_path>\n", argv[0]);

    const char *sample_video = argv[1];
    printf("=== Watch-Later Subtitle Integration Test ===\n");
    printf("Sample video: %s\n\n", sample_video);

    // Setup test environment
    setup_test_files(sample_video);
    atexit(cleanup_temp_dir);

    // Phase 1: First mpv instance - load video, add subtitle, save
    ctx = mpv_create();
    if (!ctx)
        fail("Failed to create mpv\n");

    atexit(exit_cleanup);
    initialize();

    test_phase1_save_subtitle();

    // Phase 2: Verify config file (no mpv needed)
    test_phase2_verify_config();

    // Phase 3: Second mpv instance - verify subtitle restores
    test_phase3_restore_subtitle();

    // Cleanup
    const char *quit_cmd[] = {"quit", NULL};
    mpv_command(ctx, quit_cmd);
    while (mpv_wait_event(ctx, 1)->event_id != MPV_EVENT_SHUTDOWN) {}

    return 0;
}
