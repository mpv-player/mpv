local mp_globals = {
    mp = {
        fields = {
            command = {},
            commandv = {},
            command_native = {},
            command_native_async = {},
            abort_async_command = {},
            del_property = {},
            get_property = {},
            get_property_osd = {},
            get_property_bool = {},
            get_property_number = {},
            get_property_native = {},
            set_property = {},
            set_property_bool = {},
            set_property_number = {},
            set_property_native = {},
            get_time = {},
            add_key_binding = {},
            add_forced_key_binding = {},
            remove_key_binding = {},
            register_event = {},
            unregister_event = {},
            observe_property = {},
            unobserve_property = {},
            add_timeout = {},
            add_periodic_timer = {},
            get_opt = {},
            get_script_name = {},
            get_script_directory = {},
            osd_message = {},
            get_wakeup_pipe = {},
            get_next_timeout = {},
            dispatch_events = {},
            register_idle = {},
            unregister_idle = {},
            enable_messages = {},
            register_script_message = {},
            unregister_script_message = {},
            create_osd_overlay = {},
            get_osd_size = {},
            msg = {
                fields = {
                    fatal = {},
                    error = {},
                    warn = {},
                    info = {},
                    verbose = {},
                    debug = {},
                    trace = {},
                }
            },
            -- Not documented
            -- TODO: Document or remove them
            add_hook = {},
            disable_key_bindings = {},
            enable_key_bindings = {},
            find_config_file = {},
            format_time = {},
            get_mouse_pos = {},
            set_key_bindings = {},
            set_mouse_area = {},
            set_osd_ass = {},
        }
    },
    unpack = {},
}

local mp_internal = {
    mp = {
        fields = {
            -- Internal
            -- TODO: Move to mp_internal module
            ARRAY = { fields = { info = {}, type = {} }},
            MAP = { fields = { info = {}, type = {} }},
            UNKNOWN_TYPE = { fields = { info = {}, type = {} }},
            _legacy_overlay = { fields = { res_x = {}, res_y = {}, data = {}, update = {} }},
            cancel_timer = {},
            flush_keybindings = {},
            get_osd_margins = {},
            input_define_section = {},
            input_disable_section = {},
            input_enable_section = {},
            input_set_section_mouse_area = {},
            keep_running = {},
            log = {},
            raw_abort_async_command = {},
            raw_command_native_async = {},
            raw_hook_add = {},
            raw_hook_continue = {},
            raw_observe_property = {},
            raw_unobserve_property = {},
            raw_wait_event = {},
            request_event = {},
            script_name = {},
            use_suspend = {},
            wait_event = {},
        }
    }
}

std = "min+mp"
stds = { mp = { read_globals = mp_globals } }
-- mp_internal seems to be merged with mp for other files too...
files["player/lua/defaults.lua"] = { globals = mp_internal }
files["player/lua/auto_profiles.lua"] = { globals = { "p", "get" } }
max_line_length = 100

-- TODO: Remove everything below this line
local todo = {
    "player/lua/osc.lua",
    "player/lua/ytdl_hook.lua",
    "TOOLS/lua/ao-null-reload.lua",
    "TOOLS/lua/autocrop.lua",
    "TOOLS/lua/autodeint.lua",
    "TOOLS/lua/autoload.lua",
    "TOOLS/lua/command-test.lua",
    "TOOLS/lua/cycle-deinterlace-pullup.lua",
    "TOOLS/lua/nan-test.lua",
    "TOOLS/lua/observe-all.lua",
    "TOOLS/lua/osd-test.lua",
    "TOOLS/lua/status-line.lua",
    "TOOLS/lua/test-hooks.lua",
}
for _, path in ipairs(todo) do
    files[path]["allow_defined"] = true
    files[path]["max_line_length"] = 120
end
