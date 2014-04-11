mp.command('vf add @autocrop.cropdetect:lavfi=graph="cropdetect=limit=24:round=2:reset=0"')

function update_crop_handler()
   cropdetect_metadata=mp.get_property_native("vf-metadata/autocrop.cropdetect")
   mp.command(string.format('vf add @autocrop.crop:crop=%s:%s:%s:%s',
			    cropdetect_metadata['lavfi.cropdetect.w'],
			    cropdetect_metadata['lavfi.cropdetect.h'],
			    cropdetect_metadata['lavfi.cropdetect.x'],
			    cropdetect_metadata['lavfi.cropdetect.y']))
end
mp.add_key_binding("C","update_crop",update_crop_handler)
