local msg = require 'mp.msg'

local function val2str(val)
    if type(val) == "boolean" then
        if val then val = "yes" else val = "no" end
    end
    return val
end

-- converts val to type of desttypeval
local function typeconv(desttypeval, val)
    if type(desttypeval) == "boolean" then
        if val == "yes" then
            val = true
        elseif val == "no" then
            val = false
        else
            msg.error("Error: Can't convert " .. val .. " to boolean!")
            val = nil
        end
    elseif type(desttypeval) == "number" then
        if not (tonumber(val) == nil) then
            val = tonumber(val)
        else
            msg.error("Error: Can't convert " .. val .. " to number!")
            val = nil
        end
    end
    return val
end


function read_options(options, identifier)
    if identifier == nil then
        identifier = mp.get_script_name()
    end
    msg.debug("reading options for " .. identifier)

    -- read config file
    local conffilename = "lua-settings/" .. identifier .. ".conf"
    local conffile = mp.find_config_file(conffilename)
    local f = conffile and io.open(conffile,"r")
    if f == nil then
        -- config not found
        msg.verbose(conffilename .. " not found.")
    else
        -- config exists, read values
        local linecounter = 1
        for line in f:lines() do
            if string.find(line, "#") == 1 then

            else
                local eqpos = string.find(line, "=")
                if eqpos == nil then

                else
                    local key = string.sub(line, 1, eqpos-1)
                    local val = string.sub(line, eqpos+1)

                    -- match found values with defaults
                    if options[key] == nil then
                        msg.warn(conffilename..":"..linecounter..
                            " unknown key " .. key .. ", ignoring")
                    else
                        local convval = typeconv(options[key], val)
                        if convval == nil then
                            msg.error(conffilename..":"..linecounter..
                                " error converting value '" .. val ..
                                "' for key '" .. key .. "'")
                        else
                            options[key] = convval
                        end
                    end
                end
            end
            linecounter = linecounter + 1
        end
        io.close(f)
    end

    --parse command-line options
    for key, val in pairs(mp.get_property_native("options/script-opts")) do
        local prefix = identifier.."-"
        if not (string.find(key, prefix, 1, true) == nil) then
            key = string.sub(key, string.len(prefix)+1)

            -- match found values with defaults
            if options[key] == nil then
                msg.warn("script-opts: unknown key " .. key .. ", ignoring")
            else
                local convval = typeconv(options[key], val)
                if convval == nil then
                    msg.error("script-opts: error converting value '" .. val ..
                        "' for key '" .. key .. "'")
                else
                    options[key] = convval
                end
            end
        end
    end

end


