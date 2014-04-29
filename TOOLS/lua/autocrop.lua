script_name=string.gsub(mp.get_script_name(),"lua/","")
cropdetect_label=string.format("%s-cropdetect",script_name)
crop_label=string.format("%s-crop",script_name)

-- number of evenly spaced samples to take
num_samples = tonumber(mp.get_opt(string.format("%s.num_samples", script_name)))
if not num_samples then
    num_samples = 10
end

function del_filter_if_present(label)
    -- necessary because mp.command('vf del @label:filter') raises an error if the filter doesn't exist
    local vfs = mp.get_property_native('vf')
    for i,vf in pairs(vfs) do
	if vf['label'] == label then
	    table.remove(vfs, i)
	    mp.set_property_native('vf', vfs)
	    return true
	end
    end
    return false
end

function sample_start()
    -- if there's a crop filter, just remove it and exit
    if del_filter_if_present(crop_label) then
        return
    end

    --- global variables
    original_pos        = mp.get_property("time-pos")
    original_cache_size = mp.get_property("cache-size")
    original_mute       = mp.get_property("mute")
    original_loop_file  = mp.get_property("loop-file")
    samples_gathered    = 0
    crop_params         = {
	x = math.huge,
	y = math.huge,
	w = 0,
	h = 0
    }

    -- don't waste bandwidth since we only need a few frames after
    -- each seek
    mp.set_property("cache-size", 0)
    -- don't make any sound while scanning
    mp.set_property("mute", "yes")
    -- guarantee that we don't fall off the end of the file
    mp.set_property("loop-file", "yes")

    -- insert the cropdetect filter
    ret=mp.command(
	string.format(
	    'vf add @%s:lavfi=graph="cropdetect=limit=24:round=2:reset=0"',
	    cropdetect_label
	)
    )
    -- seek to first sample position
    mp.set_property("percent-pos", 100 * 0.5 / num_samples)
    -- start the sample loop
    mp.register_event('tick', sample_advance)
end

frames=0
max_frames=24
function sample_advance()
    -- Cropdetect metadata gets reset each seek.
    -- Save it and advance once we see it, otherwise wait for
    -- another frame. It typically takes two frames to appear.
    local cropdetect_metadata = mp.get_property_native(
	string.format('vf-metadata/%s', cropdetect_label)
    )
    if cropdetect_metadata then
	if cropdetect_metadata['lavfi.cropdetect.w']
	    and cropdetect_metadata['lavfi.cropdetect.h']
	    and cropdetect_metadata['lavfi.cropdetect.x']
	    and cropdetect_metadata['lavfi.cropdetect.y']
	then
	    crop_params['x'] = math.min(crop_params['x'],
					cropdetect_metadata['lavfi.cropdetect.x'])
	    crop_params['y'] = math.min(crop_params['y'],
					cropdetect_metadata['lavfi.cropdetect.y'])
	    crop_params['w'] = math.max(crop_params['w'],
					cropdetect_metadata['lavfi.cropdetect.w'])
	    crop_params['h'] = math.max(crop_params['h'],
					cropdetect_metadata['lavfi.cropdetect.h'])
	    samples_gathered=samples_gathered+1

	    if samples_gathered < num_samples then
		-- advance to next position
		mp.set_property("percent-pos",
				100 * (samples_gathered + 0.5) / num_samples)
	    else
		-- we're done, cleanup and insert the crop filter
		sample_stop()
	    end
	    -- reset the frames counter
	    frames=0
	    return
	end
        no_cropdata=false
    else
	no_cropdata=true
    end

    if frames < max_frames then
	-- increment the frame counter for this sample
	frames=frames+1
    else
	-- don't loop endlessly if the data just isn't showing up
	require 'mp.msg'
	if no_cropdata then
	    mp.msg.error(
		"No crop data. Was the cropdetect filter successfully inserted?"
	    )
            mp.msg.error(
                "Does your version of ffmpeg/libav support AVFrame metadata?"
            )
	else
	    mp.msg.error(
		"Got empty crop data. You might need to increase max_frames."
	    )
	end
        -- reset the frames counter
        frames=0
	-- clean up. the crop filter won't get inserted since we got no metadata
	sample_stop()
    end
end

function sample_stop()
    -- stop the sample loop
    mp.unregister_event(sample_advance)
    -- remove the cropdetqwect filter
    del_filter_if_present(cropdetect_label)
    -- insert the crop filter if we have reasonable data
    if crop_params['x']<math.huge
	and crop_params['y']<math.huge
	and crop_params['w']>0
	and crop_params['h']>0
    then
	mp.command(string.format('vf add @%s:crop=%d:%d:%d:%d',
				 crop_label,
				 crop_params['w'],
				 crop_params['h'],
				 crop_params['x'],
				 crop_params['y']))
    end
    -- restore the state of altered properties
    mp.set_property("cache-size",original_cache_size)
    mp.set_property("loop-file",original_loop_file)
    mp.set_property("mute",original_mute)
    mp.set_property("time-pos",original_pos)
end

mp.add_key_binding("C","auto_crop",sample_start)
