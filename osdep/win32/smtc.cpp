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

#include "smtc.h"

#include <chrono>
#include <format>
#include <utility>

#include <windows.h>
#include <systemmediatransportcontrolsinterop.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.h>

extern "C" {
#include "common/msg.h"
#include "osdep/threads.h"
#include "player/client.h"
}

EXTERN_C IMAGE_DOS_HEADER __ImageBase;
#define WM_MP_EVENT (WM_USER + 1)

using namespace std::chrono_literals;
using namespace winrt::Windows::Media;
using winrt::Windows::Foundation::TimeSpan;

struct mpv_deleter {
    void operator()(void *ptr) const {
        mpv_free(ptr);
    }
};
using mp_string = std::unique_ptr<char, mpv_deleter>;

template<mpv_format F> struct mp_fmt;
template<> struct mp_fmt<MPV_FORMAT_FLAG>   { using type = int; };
template<> struct mp_fmt<MPV_FORMAT_INT64>  { using type = int64_t; };
template<> struct mp_fmt<MPV_FORMAT_DOUBLE> { using type = double; };

template<mpv_format F>
static inline std::optional<typename mp_fmt<F>::type>
mp_get_property(mpv_handle *mpv, const char *name)
{
    typename mp_fmt<F>::type val;
    if (mpv_get_property(mpv, name, F, &val) != MPV_ERROR_SUCCESS)
        return std::nullopt;
    return val;
}

template<typename T> struct mp_fmt_e;
template<> struct mp_fmt_e<int> { static constexpr mpv_format value = MPV_FORMAT_FLAG; };
template<> struct mp_fmt_e<bool> { static constexpr mpv_format value = MPV_FORMAT_FLAG; };
template<> struct mp_fmt_e<int64_t> { static constexpr mpv_format value = MPV_FORMAT_INT64; };
template<> struct mp_fmt_e<double> { static constexpr mpv_format value = MPV_FORMAT_DOUBLE; };

template<typename T>
static inline int mp_set_property(mpv_handle *mpv, const char *name, T &&val)
{
    using val_t = std::remove_reference_t<T>;
    typename mp_fmt<mp_fmt_e<val_t>::value>::type mpv_val = std::forward<T>(val);
    return mpv_set_property(mpv, name, mp_fmt_e<val_t>::value, &mpv_val);
}

struct smtc_ctx {
    mp_log *log;
    mpv_handle *mpv;
    SystemMediaTransportControls smtc{ nullptr };
    std::atomic_bool close{ false };
    std::atomic<HWND> hwnd{ nullptr };
};

static void update_state(SystemMediaTransportControls &smtc, mpv_handle *mpv)
{
    auto closed = mp_get_property<MPV_FORMAT_FLAG>(mpv, "idle-active");
    if (!closed.value_or(false)) {
        auto paused = mp_get_property<MPV_FORMAT_FLAG>(mpv, "pause");
        smtc.PlaybackStatus(paused.value_or(true) ? MediaPlaybackStatus::Paused : MediaPlaybackStatus::Playing);
        smtc.IsPlayEnabled(true);
        smtc.IsPauseEnabled(true);
        smtc.IsStopEnabled(true);
        auto ch_index = mp_get_property<MPV_FORMAT_INT64>(mpv, "chapter");
        auto ch_count = mp_get_property<MPV_FORMAT_INT64>(mpv, "chapter-list/count");
        auto pl_count = mp_get_property<MPV_FORMAT_INT64>(mpv, "playlist-count");
        smtc.IsNextEnabled(pl_count > 1 || ch_count > ch_index.value_or(0));
        smtc.IsPreviousEnabled(pl_count > 1 || ch_index > 0);
        smtc.IsRewindEnabled(true);
    } else {
        smtc.PlaybackStatus(MediaPlaybackStatus::Closed);
        smtc.IsPlayEnabled(false);
        smtc.IsPauseEnabled(false);
        smtc.IsStopEnabled(false);
        smtc.IsNextEnabled(false);
        smtc.IsPreviousEnabled(false);
        smtc.IsRewindEnabled(false);
    }

    auto shuffle = mp_get_property<MPV_FORMAT_FLAG>(mpv, "shuffle");
    smtc.ShuffleEnabled(shuffle.value_or(false));

    auto speed = mp_get_property<MPV_FORMAT_DOUBLE>(mpv, "speed");
    smtc.PlaybackRate(speed.value_or(1.0));

    mp_string loop_file_opt{ mpv_get_property_string(mpv, "loop-file") };
    bool loop_file = loop_file_opt && strcmp(loop_file_opt.get(), "no");
    mp_string loop_playlist_opt{ mpv_get_property_string(mpv, "loop-playlist") };
    bool loop_playlist = loop_playlist_opt && strcmp(loop_playlist_opt.get(), "no");
    if (loop_file) {
        smtc.AutoRepeatMode(MediaPlaybackAutoRepeatMode::Track);
    } else if (loop_playlist) {
        smtc.AutoRepeatMode(MediaPlaybackAutoRepeatMode::List);
    } else {
        smtc.AutoRepeatMode(MediaPlaybackAutoRepeatMode::None);
    }

    auto pos = mp_get_property<MPV_FORMAT_DOUBLE>(mpv, "time-pos");
    auto duration = mp_get_property<MPV_FORMAT_DOUBLE>(mpv, "duration");

    if (!pos || !duration)
        return;

    SystemMediaTransportControlsTimelineProperties tl;
    tl.StartTime(0s);
    tl.MinSeekTime(0s);
    tl.Position(std::chrono::duration_cast<TimeSpan>(std::chrono::duration<double>(*pos)));
    tl.MaxSeekTime(std::chrono::duration_cast<TimeSpan>(std::chrono::duration<double>(*duration)));
    tl.EndTime(std::chrono::duration_cast<TimeSpan>(std::chrono::duration<double>(*duration)));
    smtc.UpdateTimelineProperties(tl);
}

static void update_metadata(SystemMediaTransportControls &smtc, smtc_ctx &ctx)
{
    auto *mpv = ctx.mpv;
    auto updater = smtc.DisplayUpdater();
    updater.ClearAll();

    auto image_opt = mp_get_property<MPV_FORMAT_FLAG>(mpv, "current-tracks/video/image");
    bool video = bool(image_opt);
    bool image = image_opt.value_or(false);
    auto audio = mp_get_property<MPV_FORMAT_FLAG>(mpv, "current-tracks/audio/selected");

    if (!video && !image && !audio)
        return;

    mp_string title{ mpv_get_property_osd_string(mpv, "media-title") };
    if (video && !image) {
        updater.Type(MediaPlaybackType::Video);
        const auto &props = updater.VideoProperties();
        if (title)
            props.Title(winrt::to_hstring(title.get()));
        auto ch_index = mp_get_property<MPV_FORMAT_INT64>(mpv, "chapter").value_or(-1);
        if (ch_index >= 0) {
            mp_string ch_title {
                mpv_get_property_string(mpv, std::format("chapter-list/{}/title", ch_index).c_str())
            };
            if (ch_title) {
                auto ch_count = mp_get_property<MPV_FORMAT_INT64>(mpv, "chapter-list/count").value_or(0);
                props.Subtitle(winrt::to_hstring(std::format("{} ({}/{})", ch_title.get(), ch_index + 1, ch_count)));
            }
        }
    } else if (image && !audio) {
        updater.Type(MediaPlaybackType::Image);
        const auto &props = updater.ImageProperties();
        if (title)
            props.Title(winrt::to_hstring(title.get()));
    } else {
        updater.Type(MediaPlaybackType::Music);
        const auto &props = updater.MusicProperties();
        if (title)
            props.Title(winrt::to_hstring(title.get()));
        if (mp_string str{ mpv_get_property_string(mpv, "metadata/by-key/Album_Artist") })
            props.AlbumArtist(winrt::to_hstring(str.get()));
        if (mp_string str{ mpv_get_property_string(mpv, "metadata/by-key/Album") })
            props.AlbumTitle(winrt::to_hstring(str.get()));
        if (mp_string str{ mpv_get_property_string(mpv, "metadata/by-key/Album_Track_Count") })
            props.AlbumTrackCount(std::atoi(str.get()));
        if (mp_string str{ mpv_get_property_string(mpv, "metadata/by-key/Artist") })
            props.Artist(winrt::to_hstring(str.get()));
        if (mp_string str{ mpv_get_property_string(mpv, "metadata/by-key/Track") })
            props.TrackNumber(std::atoi(str.get()));
    }

    updater.Update();
}

static void handle_mp_event(smtc_ctx *ctx, mpv_event *event)
{
    if (!ctx || !ctx->smtc || !ctx->mpv || ctx->close)
        return;

    try {
        update_state(ctx->smtc, ctx->mpv);
        if (event->event_id == MPV_EVENT_PROPERTY_CHANGE) {
            auto &prop = *static_cast<mpv_event_property *>(event->data);
            if (!strcmp(prop.name, "time-pos") || !strcmp(prop.name, "duration"))
                return;
        }
        update_metadata(ctx->smtc, *ctx);
    } catch (const winrt::hresult_error& e) {
        MP_VERBOSE(ctx, "%s: 0x%x - %ls\n", __func__, int32_t(e.code()), e.message().c_str());
    }
}

static LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMsg == WM_DESTROY)
        PostQuitMessage(0);

    smtc_ctx *ctx = reinterpret_cast<smtc_ctx *>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));

    switch (uMsg)
    {
    case WM_MP_EVENT:
        handle_mp_event(ctx, reinterpret_cast<mpv_event *>(lParam));
        return 0;
    case WM_SETFOCUS:
        if (!ctx)
            return 0;
        if (auto wid { mp_get_property<MPV_FORMAT_INT64>(ctx->mpv, "window-id") })
            SetFocus(HWND(*wid));
        return 0;
    case WM_ACTIVATE:
        if (!ctx)
            return 0;
        if (auto wid { mp_get_property<MPV_FORMAT_INT64>(ctx->mpv, "window-id") }) {
            if (IsIconic(HWND(*wid)))
                ShowWindow(HWND(*wid), SW_RESTORE);
            BringWindowToTop(HWND(*wid));
        }
        return 0;
    }

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

static MP_THREAD_VOID win_event_loop_fn(void *arg)
{
    mp_thread_set_name("smtc/win");
    auto &ctx = *static_cast<smtc_ctx *>(arg);
    auto *mpv = ctx.mpv;

    WNDCLASS wc = {
        .lpfnWndProc = WindowProc,
        .hInstance = HINSTANCE(&__ImageBase),
        .hIcon = LoadIconW(HINSTANCE(&__ImageBase), L"IDI_ICON1"),
        .lpszClassName = L"mpv-smtc"
    };
    RegisterClassW(&wc);

    try {
        // Dummy window is used to allow SMTC to work also in audio only mode,
        // where VO may not be created.
        ctx.hwnd = CreateWindowExW(0, wc.lpszClassName, L"mpv smtc",
                                   WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                                   CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                                   nullptr, nullptr, wc.hInstance, nullptr);
        if (!ctx.hwnd)
            winrt::throw_last_error();

        SystemMediaTransportControls &smtc = ctx.smtc;
        auto interop = winrt::get_activation_factory<SystemMediaTransportControls,
                                                     ISystemMediaTransportControlsInterop>();
        HRESULT hr = interop->GetForWindow(ctx.hwnd,
                                           winrt::guid_of<SystemMediaTransportControls>(),
                                           winrt::put_abi(smtc));
        if (FAILED(hr))
            winrt::throw_hresult(hr);
        SetWindowLongPtrW(ctx.hwnd, GWLP_USERDATA, LONG_PTR(&ctx));

        smtc.IsEnabled(true);

        smtc.ButtonPressed([&](const SystemMediaTransportControls &,
                               const SystemMediaTransportControlsButtonPressedEventArgs &args) {
            switch (args.Button()) {
            case SystemMediaTransportControlsButton::Play:
                mp_set_property(mpv, "pause", false);
                break;
            case SystemMediaTransportControlsButton::Pause:
                mp_set_property(mpv, "pause", true);
                break;
            case SystemMediaTransportControlsButton::Stop:
                mpv_command_string(mpv, "stop");
                break;
            case SystemMediaTransportControlsButton::Next: {
                auto ch_index = mp_get_property<MPV_FORMAT_INT64>(mpv, "chapter").value_or(0);
                auto ch_count = mp_get_property<MPV_FORMAT_INT64>(mpv, "chapter-list/count");
                // mpv allows to jump past last chapter
                mpv_command_string(mpv, ch_index < ch_count ? "add chapter 1" : "playlist-next");
                break;
            }
            case SystemMediaTransportControlsButton::Previous: {
                auto ch_index = mp_get_property<MPV_FORMAT_INT64>(mpv, "chapter");
                mpv_command_string(mpv, ch_index > 0 ? "add chapter -1" : "playlist-prev");
                break;
            }
            default:
                break;
            }
        });
        smtc.PlaybackPositionChangeRequested([&](const SystemMediaTransportControls &,
                                                 const PlaybackPositionChangeRequestedEventArgs &args) {
            auto position = args.RequestedPlaybackPosition();
            auto pos = std::chrono::duration_cast<std::chrono::duration<double>>(position).count();
            mp_set_property(mpv, "time-pos", pos);
        });
        smtc.PlaybackRateChangeRequested([&](const SystemMediaTransportControls &,
                                             const PlaybackRateChangeRequestedEventArgs &args) {
            mp_set_property(mpv, "speed", args.RequestedPlaybackRate());
        });
        smtc.ShuffleEnabledChangeRequested([&](const SystemMediaTransportControls &,
                                               const ShuffleEnabledChangeRequestedEventArgs &args) {
            mp_set_property(mpv, "shuffle", args.RequestedShuffleEnabled());
        });
        smtc.AutoRepeatModeChangeRequested([&](const SystemMediaTransportControls &,
                                               const AutoRepeatModeChangeRequestedEventArgs &args) {
            bool loop_file = false, loop_playlist = false;
            switch (args.RequestedAutoRepeatMode()) {
                case MediaPlaybackAutoRepeatMode::Track:
                    loop_file = true;
                    break;
                case MediaPlaybackAutoRepeatMode::List:
                    loop_playlist = true;
                    break;
                case MediaPlaybackAutoRepeatMode::None:
                    break;
            }
            mp_set_property(mpv, "loop-file", loop_file);
            mp_set_property(mpv, "loop-playlist", loop_playlist);
        });

        MSG msg;
        while(BOOL ret = GetMessageW(&msg, nullptr, 0, 0)) {
            if (ret == -1)
                winrt::throw_last_error();
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    } catch (const winrt::hresult_error& e) {
        MP_ERR(&ctx, "%s: 0x%x - %ls\n", __func__, int32_t(e.code()), e.message().c_str());
    }

    ctx.close = true;
    mpv_wakeup(mpv);
    HWND hwnd = ctx.hwnd;
    ctx.hwnd = nullptr;
    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, HINSTANCE(&__ImageBase));

    MP_THREAD_RETURN();
}

static MP_THREAD_VOID mpv_event_loop_fn(void *arg)
{
    mp_thread_set_name("smtc/mpv");
    auto mpv = static_cast<mpv_handle *>(arg);
    smtc_ctx ctx = {
        .log = mp_client_get_log(mpv),
        .mpv = mpv
    };

    // Create a dedicated window and event loop. We could use the mpv main window,
    // but it is not always available, especially in audio-only/console mode.
    mp_thread win_event_loop;
    if (mp_thread_create(&win_event_loop, win_event_loop_fn, &ctx)) {
        MP_ERR(&ctx, "Failed to create window event thread!\n");
        goto error;
    }

    // It is recommended that you keep the system controls in sync with your
    // media playback by updating these properties approximately every 5 seconds
    // during playback and again whenever the state of playback changes, such as
    // pausing or seeking to a new position.
    // https://learn.microsoft.com/windows/uwp/audio-video-camera/system-media-transport-controls
    // For simplicity we observe time-pos and duration as integers, so we get
    // update every second, faster than recommended, but should be fine.

    mpv_observe_property(mpv, 0, "current-tracks", MPV_FORMAT_NODE_MAP);
    mpv_observe_property(mpv, 0, "duration", MPV_FORMAT_INT64);
    mpv_observe_property(mpv, 0, "idle-active", MPV_FORMAT_FLAG);
    mpv_observe_property(mpv, 0, "media-title", MPV_FORMAT_STRING);
    mpv_observe_property(mpv, 0, "metadata", MPV_FORMAT_NODE_MAP);
    mpv_observe_property(mpv, 0, "pause", MPV_FORMAT_FLAG);
    mpv_observe_property(mpv, 0, "shuffle", MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv, 0, "speed", MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv, 0, "time-pos", MPV_FORMAT_INT64);
    // TODO: Options are not observable, fix me!
    mpv_observe_property(mpv, 0, "loop-file", MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv, 0, "loop-playlist", MPV_FORMAT_DOUBLE);

    while (!ctx.close) {
        mpv_event *event = mpv_wait_event(mpv, -1);
        if (ctx.close)
            break;
        if (event->event_id == MPV_EVENT_SHUTDOWN) {
            HWND hwnd = ctx.hwnd;
            if (hwnd)
                PostMessageW(hwnd, WM_CLOSE, 0, 0);
            break;
        }
        if (event->event_id == MPV_EVENT_PROPERTY_CHANGE ||
            event->event_id == MPV_EVENT_PLAYBACK_RESTART)
        {
            HWND hwnd = ctx.hwnd;
            if (hwnd)
                SendMessageW(hwnd, WM_MP_EVENT, 0, LPARAM(event));
        }
    }
    mp_thread_join(win_event_loop);

error:
    mpv_destroy(mpv);
    MP_THREAD_RETURN();
}

void mp_smtc_init(mpv_handle *mpv)
{
    mp_thread mpv_event_loop;
    if (!mp_thread_create(&mpv_event_loop, mpv_event_loop_fn, mpv))
        mp_thread_detach(mpv_event_loop);
}
