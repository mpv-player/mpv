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

#include "w32_register.h"

#include <windows.h>
#include <knownfolders.h>
#include <pathcch.h>
#include <shlobj.h>
#include <shlwapi.h>

#include <common/msg.h>
#include <options/options.h>
#include <osdep/io.h>
#include <player/core.h>
#include <stream/stream.h>
#include <version.h>

#include "windows_utils.h"

#define MPV_NAME L"mpv"
#define MPV_FRIENDLY_NAME L"mpv media player"

#define KEY_AUTOPLAY L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\AutoplayHandlers"
#define KEY_MPV_APP L"Software\\Classes\\Applications\\" MPV_NAME L".exe"
#define KEY_MPV_APP_PATHS L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\"
#define KEY_MPV_APP_PATH(suffix) KEY_MPV_APP_PATHS MPV_NAME suffix
#define KEY_MPV_CAPABILITIES_APP L"Software\\Clients\\Media\\" MPV_NAME
#define KEY_MPV_CAPABILITIES KEY_MPV_CAPABILITIES_APP L"\\Capabilities"
#define KEY_MPV_UNINSTALL L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\" MPV_NAME

#define MPV_PROG_ID_PREFIX L"io.mpv."
#define MPV_PROG_ID(h) MPV_PROG_ID_PREFIX h
#define KEY_MPV_PROG_ID(h) L"Software\\Classes\\" MPV_PROG_ID(h)

struct w32_register_opts {
    bool register_opt;
    char *rpath;
    bool unregister;
};

#define OPT_BASE_STRUCT struct w32_register_opts
const struct m_sub_options w32_register_conf = {
    .opts = (const struct m_option[]) {
        {"register", OPT_BOOL(register_opt)},
        {"register-rpath", OPT_STRING(rpath)},
        {"unregister", OPT_BOOL(unregister)},
        {0}
    },
    .size = sizeof(struct w32_register_opts)
};

static bool is_admin(void)
{
    PSID sid;
    SID_IDENTIFIER_AUTHORITY identifier_authority = SECURITY_NT_AUTHORITY;
    if (!AllocateAndInitializeSid(&identifier_authority, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                  DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &sid))
    {
        return false;
    }

    BOOL member = false;
    CheckTokenMembership(NULL, sid, &member);
    if (sid)
        FreeSid(sid);

    return member;
}

static void create_shortcut(struct mp_log *log, const wchar_t *target,
                            const wchar_t *shortcut)
{
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        mp_msg(log, MSGL_ERR, "Failed to initialize COM: %s\n",
               mp_HRESULT_to_str(hr));
        return;
    }

    IShellLinkW *shell_link = NULL;
    IPersistFile *persist_file = NULL;
    hr = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                          &IID_IShellLinkW, (void **)&shell_link);
    if (FAILED(hr)) {
        mp_msg(log, MSGL_ERR, "Failed to create IShellLinkW instance: %s\n",
               mp_HRESULT_to_str(hr));
        goto done;
    }

    hr = IShellLinkW_SetPath(shell_link, target);
    if (FAILED(hr)) {
        mp_msg(log, MSGL_ERR, "Failed to set target path: %s\n",
               mp_HRESULT_to_str(hr));
        goto done;
    }

    hr = IShellLinkW_QueryInterface(shell_link, &IID_IPersistFile,
                                    (LPVOID *)&persist_file);
    if (FAILED(hr)) {
        mp_msg(log, MSGL_ERR, "Failed to get IPersistFile interface: %s\n",
               mp_HRESULT_to_str(hr));
        goto done;
    }

    hr = IPersistFile_Save(persist_file, shortcut, TRUE);
    if (FAILED(hr)) {
        mp_msg(log, MSGL_ERR, "Failed to save shortcut: %s\n",
               mp_HRESULT_to_str(hr));
        goto done;
    }

    mp_msg(log, MSGL_V, "Shortcut created: '%ls' -> '%ls'\n", shortcut, target);

done:
    SAFE_RELEASE(persist_file);
    SAFE_RELEASE(shell_link);
    CoUninitialize();
}

static void reg_add(struct mp_log *log, HKEY key, LPCWSTR sub_key, LPCWSTR name,
                    DWORD type, const BYTE *data, DWORD size)
{
    HKEY sub_key_handle;
    if (RegCreateKeyExW(key, sub_key, 0, NULL, REG_OPTION_NON_VOLATILE,
                        KEY_WRITE, NULL, &sub_key_handle,
                        NULL) != ERROR_SUCCESS)
    {
        mp_msg(log, MSGL_ERR, "Failed to create registry key\n");
        return;
    }

    if (RegSetValueExW(sub_key_handle, name, 0, type, data, size) != ERROR_SUCCESS)
        mp_msg(log, MSGL_ERR, "Failed to set registry value\n");

    RegCloseKey(sub_key_handle);
}

static void reg_add_str(struct mp_log *log, HKEY key, LPCWSTR sub_key,
                        LPCWSTR name, LPCWSTR value)
{
    const wchar_t *root = key == HKEY_LOCAL_MACHINE ? L"HKEY_LOCAL_MACHINE"
                                                    : L"HKEY_CURRENT_USER";
    mp_msg(log, MSGL_V, "Adding registry key: %ls\\%ls\\%ls -> '%ls'\n", root,
           sub_key, name ? name : L"(Default)", value);
    reg_add(log, key, sub_key, name, REG_SZ, (const BYTE *)value,
            (wcslen(value) + 1) * sizeof(WCHAR));
}

static void reg_add_dwr(struct mp_log *log, HKEY key, LPCWSTR sub_key,
                        LPCWSTR name, DWORD value)
{
    const wchar_t *root = key == HKEY_LOCAL_MACHINE ? L"HKEY_LOCAL_MACHINE"
                                                    : L"HKEY_CURRENT_USER";
    mp_msg(log, MSGL_V, "Adding registry key: %ls\\%ls\\%ls -> %lu\n", root,
           sub_key, name ? name : L"(Default)", value);
    reg_add(log, key, sub_key, name, REG_DWORD, (const BYTE *)&value,
            sizeof(DWORD));
}

static void reg_del(struct mp_log *log, HKEY key, LPCWSTR sub_key,
                    LPCWSTR value)
{
    const wchar_t *root = key == HKEY_LOCAL_MACHINE ? L"HKEY_LOCAL_MACHINE"
                                                    : L"HKEY_CURRENT_USER";
    const char *name    = value ? "value" : "key";
    mp_msg(log, MSGL_V, "Removing registry %s: %ls\\%ls%ls%ls\n", name, root,
           sub_key, value ? L"\\" : L"", value ? value : L"");

    LSTATUS status = value ? RegDeleteKeyValueW(key, sub_key, value)
                           : RegDeleteTreeW(key, sub_key);
    if (status != ERROR_SUCCESS) {
        mp_msg(log, status == ERROR_FILE_NOT_FOUND ? MSGL_DEBUG : MSGL_ERR,
               "Failed to delete registry %s: %ls\\%ls%ls%ls -> %s\n", name,
               root, sub_key, value ? L"\\" : L"", value ? value : L"",
               mp_HRESULT_to_str(HRESULT_FROM_WIN32(status)));
    }
}

static uint64_t get_file_size(const wchar_t *path)
{
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExW(path, GetFileExInfoStandard, &fad))
        return 0;

    return ((uint64_t)fad.nFileSizeHigh << 32) | fad.nFileSizeLow;
}

static wchar_t *w32_get_shortcut_path(struct mp_log *log)
{
    LPWSTR shortcut_path = NULL;
    LPWSTR programs_path = NULL;
    if (SUCCEEDED(SHGetKnownFolderPath(&FOLDERID_Programs, 0, NULL, &programs_path))) {
        PathAllocCombine(programs_path, MPV_NAME L".lnk",
                         PATHCCH_ALLOW_LONG_PATHS, &shortcut_path);
    } else {
        mp_msg(log, MSGL_ERR, "Failed to get Programs folder path.\n");
    }
    CoTaskMemFree(programs_path);
    return shortcut_path;
}

#define REGISTER_AUTOPLAY_HANDLER(media, event, action)                        \
    reg_add_str(log, root, KEY_MPV_PROG_ID(media) L"\\shell", NULL, L"open");  \
    reg_add_str(log, root, KEY_MPV_PROG_ID(media) L"\\shell\\open\\command",   \
                NULL, L"mpv.exe " media L":// --" media L"-device=%L");        \
    reg_add_str(log, root,                                                     \
                KEY_AUTOPLAY L"\\Handlers\\MpvPlay" event "OnArrival",         \
                L"Action", L"" action);                                        \
    reg_add_str(log, root,                                                     \
                KEY_AUTOPLAY L"\\Handlers\\MpvPlay" event "OnArrival",         \
                L"DefaultIcon", icon_path);                                    \
    reg_add_str(log, root,                                                     \
                KEY_AUTOPLAY L"\\Handlers\\MpvPlay" event "OnArrival",         \
                L"InvokeProgID", MPV_PROG_ID(media));                          \
    reg_add_str(log, root,                                                     \
                KEY_AUTOPLAY L"\\Handlers\\MpvPlay" event "OnArrival",         \
                L"InvokeVerb", L"open");                                       \
    reg_add_str(log, root,                                                     \
                KEY_AUTOPLAY L"\\Handlers\\MpvPlay" event "OnArrival",         \
                L"Provider", MPV_FRIENDLY_NAME);                               \
    reg_add_str(log, root,                                                     \
                KEY_AUTOPLAY L"\\EventHandlers\\Play" event "OnArrival",       \
                L"MpvPlay" event "OnArrival", L"");

static void w32_register(struct MPContext *mpctx)
{
    void *tmp = talloc_new(NULL);
    struct mp_log *log = mp_log_new(tmp, mpctx->log, "win32");

    LPWSTR mpv_path = talloc_array(tmp, wchar_t, MP_PATH_MAX);
    DWORD mpv_path_len = GetModuleFileNameW(NULL, mpv_path, MP_PATH_MAX);
    if (!mpv_path_len || mpv_path_len == MP_PATH_MAX) {
        mp_msg(log, MSGL_ERR, "Failed to get mpv path.\n");
        return;
    }
    LPWSTR icon_path = talloc_array(tmp, wchar_t, mpv_path_len + 3);
    memcpy(icon_path, mpv_path, mpv_path_len * sizeof(wchar_t));
    icon_path[mpv_path_len + 0] = L',';
    icon_path[mpv_path_len + 1] = L'0';
    icon_path[mpv_path_len + 2] = L'\0';

    HKEY root = is_admin() ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
    mp_msg(log, MSGL_INFO, "Registering mpv for %s...\n",
           root == HKEY_LOCAL_MACHINE ? "all users" : "the current user");

    // Register mpv as an application
    // <https://learn.microsoft.com/windows/win32/shell/app-registration>
    reg_add_str(log, root, KEY_MPV_APP_PATH(".exe"), NULL, mpv_path);
    reg_add_dwr(log, root, KEY_MPV_APP_PATH(".exe"), L"UseUrl", 1);

    char *rpath = mpctx->opts->w32_register_opts->rpath;
    if (rpath) {
        wchar_t *rpath_w = mp_from_utf8(tmp, rpath);
        reg_add_str(log, root, KEY_MPV_APP_PATH(".exe"), L"Path", rpath_w);
        reg_add_str(log, root, KEY_MPV_APP_PATH(".com"), L"Path", rpath_w);
    }

    // Name of the "Open With" entry in the context menu
    // I prefer a simple "mpv" here, so let's skip this.
    // reg_add_str(log, root, KEY_MPV_APP, L"FriendlyAppName", MPV_FRIENDLY_NAME);

    // Add shell open command
    reg_add_str(log, root, KEY_MPV_APP L"\\shell", NULL, L"open");
    reg_add_str(log, root, KEY_MPV_APP L"\\shell\\open\\command", NULL,
                L"mpv.exe -- \"%L\"");

    // Register mpv capabilities and handlers

    // Note that we do not register any extensions or protocols directly,
    // as mpv does not own any. It is simply a media player that can open them.
    // Windows will prompt the user to choose the default application for each
    // file type encountered.

    // Register only a single URL and file handler; there is no need to
    // duplicate this for each supported protocol or file type.

    // Register URL handler
    reg_add_str(log, root, KEY_MPV_PROG_ID("url"), NULL, L"URL:mpv");
    reg_add_str(log, root, KEY_MPV_PROG_ID("url"), L"URL Protocol", L"");
    reg_add_dwr(log, root, KEY_MPV_PROG_ID("url"), L"EditFlags", FTA_Show);
    reg_add_str(log, root, KEY_MPV_PROG_ID("url"), L"FriendlyTypeName",
                L"mpv URL handler");
    reg_add_str(log, root, KEY_MPV_PROG_ID("url") L"\\shell", NULL, L"open");
    reg_add_str(log, root, KEY_MPV_PROG_ID("url") L"\\shell\\open\\command",
                NULL, L"mpv.exe -- \"%L\"");
    reg_add_str(log, root, KEY_MPV_PROG_ID("url") L"\\shell\\open\\ddeexec",
                NULL, L"");

    // Register URL protocols
    char **safe_protocols = talloc_steal(tmp, stream_get_proto_list(true));
    mp_assert(safe_protocols);
    char *protocols_str = NULL;
    for (int i = 0; safe_protocols[i]; ++i) {
        reg_add_str(log, root, KEY_MPV_CAPABILITIES L"\\URLAssociations",
                    mp_from_utf8(tmp, safe_protocols[i]),
                    MPV_PROG_ID("url"));
        if (!protocols_str) {
            protocols_str = talloc_strdup(tmp, safe_protocols[i]);
        } else {
            protocols_str = talloc_asprintf_append(protocols_str, ":%s",
                                                   safe_protocols[i]);
        }
    }
    if (protocols_str) {
        reg_add_str(log, root, KEY_MPV_APP_PATH(".exe"), L"SupportedProtocols",
                    mp_from_utf8(tmp, protocols_str));
    }

    // Register file handler
    reg_add_str(log, root, KEY_MPV_PROG_ID("file"), NULL, L"mpv");
    reg_add_str(log, root, KEY_MPV_PROG_ID("file"), L"FriendlyTypeName", L"mpv Media File");
    reg_add_dwr(log, root, KEY_MPV_PROG_ID("file"), L"EditFlags", FTA_OpenIsSafe);
    reg_add_str(log, root, KEY_MPV_PROG_ID("file") L"\\shell", NULL, L"open");
    reg_add_str(log, root, KEY_MPV_PROG_ID("file") L"\\shell\\open\\command",
                NULL, L"mpv.exe -- \"%L\"");

    // Register file associations
    char **exts_groups[] = {
#if HAVE_LIBARCHIVE
        mpctx->opts->archive_exts,
#endif
        mpctx->opts->audio_exts,
        mpctx->opts->image_exts,
        mpctx->opts->playlist_exts,
        mpctx->opts->video_exts,
    };
    for (int i = 0; i < MP_ARRAY_SIZE(exts_groups); ++i) {
        char **exts = exts_groups[i];
        for (int j = 0; exts && exts[j]; ++j) {
            wchar_t *ext = mp_from_utf8(tmp, mp_tprintf(10, ".%s", exts[j]));
            reg_add_str(log, root, KEY_MPV_APP L"\\SupportedTypes", ext, L"");
            reg_add_str(log, root, KEY_MPV_CAPABILITIES L"\\FileAssociations",
                        ext, MPV_PROG_ID("file"));
        }
    }

    // Connect above handlers to the application
    reg_add_str(log, root, KEY_MPV_CAPABILITIES, L"ApplicationName", MPV_NAME);
    reg_add_str(log, root, KEY_MPV_CAPABILITIES, L"ApplicationDescription", MPV_FRIENDLY_NAME);

    // Register the application
    reg_add_str(log, root, L"Software\\RegisteredApplications", MPV_NAME, KEY_MPV_CAPABILITIES);

    // <https://learn.microsoft.com/windows/win32/shell/how-to-register-an-event-handler>

#if HAVE_CDDA
    // Register CDDA handler
    REGISTER_AUTOPLAY_HANDLER("cdda", "CDAudio", "Play CD-Audio");
#endif

#if HAVE_DVDNAV
    // Register DVD handler
    REGISTER_AUTOPLAY_HANDLER("dvd", "DVDMovie", "Play DVD movie");
#endif

#if HAVE_LIBBLURAY
    // Register Blu-ray handler
    REGISTER_AUTOPLAY_HANDLER("bluray", "BluRay", "Play Blu-ray movie");
#endif

    // Create a shortcut in the Start Menu
    // Note that this is required for SystemMediaTransportControls to detect the
    // app correctly. Which is quite stupid, but it's the only way to make it work.
    wchar_t *shortcut_path = w32_get_shortcut_path(log);
    if (shortcut_path) {
        create_shortcut(log, mpv_path, shortcut_path);
        LocalFree(shortcut_path);
    }

    // Register uninstaller
    // <https://learn.microsoft.com/windows/win32/msi/uninstall-registry-key>
    reg_add_str(log, root, KEY_MPV_UNINSTALL, L"DisplayName", L"mpv media player");
    reg_add_str(log, root, KEY_MPV_UNINSTALL, L"DisplayIcon", icon_path);
    reg_add_str(log, root, KEY_MPV_UNINSTALL, L"DisplayVersion", L"" VERSION);
    reg_add_str(log, root, KEY_MPV_UNINSTALL, L"Version", L"" VERSION);
    reg_add_dwr(log, root, KEY_MPV_UNINSTALL, L"Language", 1033);
    reg_add_str(log, root, KEY_MPV_UNINSTALL, L"UninstallString", L"mpv.exe --no-config --unregister");
    reg_add_dwr(log, root, KEY_MPV_UNINSTALL, L"NoModify", 1);
    reg_add_dwr(log, root, KEY_MPV_UNINSTALL, L"NoRepair", 1);
    reg_add_str(log, root, KEY_MPV_UNINSTALL, L"Publisher", L"mpv");
    reg_add_str(log, root, KEY_MPV_UNINSTALL, L"HelpLink", L"https://mpv.io/manual");
    reg_add_str(log, root, KEY_MPV_UNINSTALL, L"Readme", L"https://mpv.io/community");
    reg_add_str(log, root, KEY_MPV_UNINSTALL, L"URLInfoAbout", L"https://mpv.io");
    reg_add_str(log, root, KEY_MPV_UNINSTALL, L"URLUpdateInfo", L"https://mpv.io/installation");

    if (SUCCEEDED(PathCchRemoveFileSpec(mpv_path, MP_PATH_MAX)))
        reg_add_str(log, root, KEY_MPV_UNINSTALL, L"InstallLocation", mpv_path);

    uint64_t size = get_file_size(mpv_path);
    if (size)
        reg_add_dwr(log, root, KEY_MPV_UNINSTALL, L"EstimatedSize", size);

    mp_msg(log, MSGL_INFO, "mpv has been successfully registered.\n");
    mp_msg(log, MSGL_INFO, "Note: The mpv binary has not been copied or moved. Please keep it in the current directory.\n");
    mp_msg(log, MSGL_INFO, "If you move it, simply run this process again to re-register.\n");

    talloc_free(tmp);
}

static void w32_unregister(struct MPContext *mpctx)
{
    struct mp_log *log = mp_log_new(NULL, mpctx->log, "win32");

    HKEY root = is_admin() ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
    mp_msg(log, MSGL_INFO, "Unregistering mpv for %s...\n",
           root == HKEY_LOCAL_MACHINE ? "all users" : "the current user");

    reg_del(log, root, KEY_MPV_APP_PATH(".exe"), NULL);
    reg_del(log, root, KEY_MPV_APP_PATH(".com"), NULL);
    reg_del(log, root, KEY_MPV_APP, NULL);
    reg_del(log, root, KEY_MPV_CAPABILITIES_APP, NULL);
    reg_del(log, root, L"Software\\RegisteredApplications", MPV_NAME);

    reg_del(log, root, KEY_MPV_PROG_ID("bluray"), NULL);
    reg_del(log, root, KEY_MPV_PROG_ID("cdda"), NULL);
    reg_del(log, root, KEY_MPV_PROG_ID("dvd"), NULL);
    reg_del(log, root, KEY_MPV_PROG_ID("file"), NULL);
    reg_del(log, root, KEY_MPV_PROG_ID("url"), NULL);

    reg_del(log, root, KEY_AUTOPLAY L"\\Handlers\\MpvPlayCDAudioOnArrival", NULL);
    reg_del(log, root, KEY_AUTOPLAY L"\\EventHandlers\\PlayCDAudioOnArrival",
            L"MpvPlayCDAudioOnArrival");
    reg_del(log, root, KEY_AUTOPLAY L"\\Handlers\\MpvPlayDVDMovieOnArrival", NULL);
    reg_del(log, root, KEY_AUTOPLAY L"\\EventHandlers\\PlayDVDMovieOnArrival",
            L"MpvPlayDVDMovieOnArrival");
    reg_del(log, root, KEY_AUTOPLAY L"\\Handlers\\MpvPlayBluRayOnArrival", NULL);
    reg_del(log, root, KEY_AUTOPLAY L"\\EventHandlers\\PlayBluRayOnArrival",
            L"MpvPlayBluRayOnArrival");

    wchar_t *shortcut_path = w32_get_shortcut_path(log);
    if (shortcut_path) {
        mp_msg(log, MSGL_V, "Removing shortcut: '%ls'\n", shortcut_path);
        if (!DeleteFileW(shortcut_path)) {
            DWORD err = GetLastError();
            mp_msg(log, err == ERROR_FILE_NOT_FOUND ? MSGL_DEBUG : MSGL_ERR,
                   "Failed to delete shortcut: '%ls' -> %s\n", shortcut_path,
                   mp_LastError_to_str());
        }
        LocalFree(shortcut_path);
    }

    reg_del(log, root, KEY_MPV_UNINSTALL, NULL);

    mp_msg(log, MSGL_INFO, "mpv has been successfully unregistered.\n");
    mp_msg(log, MSGL_INFO, "You may now safely delete the mpv binary if desired.\n");

    talloc_free(log);
}

bool mp_w32_handle_register(struct MPContext *mpctx)
{
    struct w32_register_opts *opts = mpctx->opts->w32_register_opts;
    if (!opts->register_opt && !opts->unregister)
        return false;

    if (opts->register_opt) {
        w32_register(mpctx);
    } else if (opts->unregister) {
        w32_unregister(mpctx);
    }

    return true;
}
