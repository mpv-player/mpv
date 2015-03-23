--sets the startime of a youtube video as specified in the "t=HHhMMmSSs" part of the url
--NOTE: This might become obsolete once youtube-dl adds the functionality

local msg = require 'mp.msg'

function youtube_starttime()
  url = mp.get_property("path", "")
  start = 0

  if string.find(url, "youtu%.?be") and
    ((url:find("http://") == 1) or (url:find("https://") == 1)) then
      time = string.match(url, "[#&%?]t=%d*h?%d*m?%d+s?m?h?")
      --the time-string can start with #, & or ? followed by t= and the timing parameters
      --at least one number needs to be present after t=, followed by h, m, s or nothing (>implies s)

      if time then
        for pos in string.gmatch(time,"%d+%a?") do
          if string.match(pos,"%d+h") then            --find out multiplier for
            multiplier = 60*60                        --hours
          elseif string.match(pos,"%d+m") then
            multiplier = 60                           --minutes
          else multiplier = 1 end                     --seconds

          start = start + (string.match(pos,"%d+") * multiplier)
        end

        msg.info("parsed '" .. time .. "' into '" .. start .. "' seconds")
      end

      mp.set_property("file-local-options/start",start)
  end
end

mp.add_hook("on_load", 50, youtube_starttime)
