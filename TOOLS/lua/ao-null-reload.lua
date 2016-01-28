-- Handles the edge case where previous attempts to init audio have failed, but
-- might start working due to a newly added device. This is required in
-- particular for ao=wasapi, since the internal IMMNotificationClient code that
-- normally triggers ao-reload will not be running in this case.

function do_reload()
    mp.command("ao-reload")
    reloading = nil
end

function on_audio_device_list_change()
    if mp.get_property("current-ao") == "null" and not reloading then
        mp.msg.verbose("audio-device-list changed: reloading audio")
        -- avoid calling ao-reload too often
        reloading = mp.add_timeout(0.5, do_reload)
    end
end

mp.set_property("options/audio-fallback-to-null", "yes")
mp.observe_property("audio-device-list", "native", on_audio_device_list_change)
