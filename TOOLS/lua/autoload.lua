-- This script automatically loads playlist entries before and after the
-- the currently played file. It does so by scanning the directory a file is
-- located in when starting playback. It sorts the directory entries
-- alphabetically, and adds entries before and after the current file to
-- the internal playlist. (It stops if the it would add an already existing
-- playlist entry at the same position - this makes it "stable".)
-- Add at most 5 * 2 files when starting a file (before + after).
MAXENTRIES = 5

mputils = require 'mp.utils'

function add_files_at(index, files)
    index = index - 1
    local oldcount = mp.get_property_number("playlist-count", 1)
    for i = 1, #files do
        mp.commandv("loadfile", files[i], "append")
        mp.commandv("playlist_move", oldcount + i - 1, index + i - 1)
    end
end

function find_and_add_entries()
    local path = mp.get_property("path", "")
    local dir, filename = mputils.split_path(path)
    if #dir == 0 then
        return
    end
    local files = mputils.readdir(dir, "files")
    table.sort(files)
    local pl = mp.get_property_native("playlist", {})
    local pl_current = mp.get_property_number("playlist-pos", 0) + 1
    -- Find the current pl entry (dir+"/"+filename) in the sorted dir list
    local current
    for i = 1, #files do
        if files[i] == filename then
            current = i
            break
        end
    end
    if current == nil then
        return
    end
    local append = {[-1] = {}, [1] = {}}
    for dir = -1, 1, 2 do -- 2 iterations, with dir = -1 and +1
        for i = 1, MAXENTRIES do
            local file = files[current + i * dir]
            local pl_e = pl[pl_current + i * dir]
            if file == nil or file[1] == "." then
                break
            end
            if pl_e then
                -- If there's a playlist entry, and it's the same file, stop.
                if pl_e.filename == file then
                    break
                end
            end
            if dir == -1 then
                if pl_current == 1 then -- never add additional entries in the middle
                    mp.msg.info("Prepending " .. file)
                    table.insert(append[-1], 1, file)
                end
            else
                mp.msg.info("Adding " .. file)
                table.insert(append[1], file)
            end
        end
    end
    add_files_at(pl_current + 1, append[1])
    add_files_at(pl_current, append[-1])
end

mp.register_event("start-file", find_and_add_entries)
