/*
 * config for cfgparser
 */

struct config conf[]={
	/* name, pointer, type, flags, min, max */
	{"vo", &video_driver, CONF_TYPE_STRING, 0, 0, 0},
	{"dsp", &dsp, CONF_TYPE_STRING, 0, 0, 0},
	{"encode", &encode_name, CONF_TYPE_STRING, 0, 0, 0},
	{"bg", &play_in_bg, CONF_TYPE_FLAG, 0, 0, 1},
	{"nobg", &play_in_bg, CONF_TYPE_FLAG, 0, 1, 0},
	{"sb", &seek_to_byte, CONF_TYPE_INT, 0, 0, 0},
	{"ss", &seek_to_sec, CONF_TYPE_INT, 0, 0, 0},
	{"sound", &has_audio, CONF_TYPE_FLAG, 0, 0, 1},
	{"nosound", &has_audio, CONF_TYPE_FLAG, 0, 1, 0},
	{"abs", &audio_buffer_size, CONF_TYPE_INT, 0, 0, 0},
	{"delay", &audio_delay, CONF_TYPE_FLOAT, 0, 0, 0},
	{"bps", &pts_from_bps, CONF_TYPE_FLAG, 0, 0, 1},
	{"nobps", &pts_from_bps, CONF_TYPE_FLAG, 0, 1, 0},
	{"alsa", &alsa, CONF_TYPE_FLAG, 0, 0, 1},
	{"noalsa", &alsa, CONF_TYPE_FLAG, 0, 1, 0},
	{"ni", &force_ni, CONF_TYPE_FLAG, 0, 0, 1},
	{"noni", &force_ni, CONF_TYPE_FLAG, 0, 1, 0},
	{"aid", &audio_id, CONF_TYPE_INT, 0, 0, 0},
	{"vid", &video_id, CONF_TYPE_INT, 0, 0, 0},
	{"auds", &avi_header.audio_codec, CONF_TYPE_STRING, 0, 0, 0},
	{"vids", &avi_header.video_codec, CONF_TYPE_STRING, 0, 0, 0},
	{"mc", &default_max_pts_correction, CONF_TYPE_FLOAT, 0, 0, 0},
	{"fps", &force_fps, CONF_TYPE_FLOAT, 0, 0, 0},
	{"afm", &audio_format, CONF_TYPE_INT, 0, 0, 0},
	{"vcd", &vcd_track, CONF_TYPE_INT, 0, 0, 0},
	{"pp", &divx_quality, CONF_TYPE_INT, 0, 0, 0},
	{"br", &encode_bitrate, CONF_TYPE_INT, 0, 0, 0},
	{"x", &screen_size_x, CONF_TYPE_INT, 0, 0, 0},
	{"y", &screen_size_y, CONF_TYPE_INT, 0, 0, 0},
	{"xy", &screen_size_xy, CONF_TYPE_INT, 0, 0, 0},
	{"fs", &fullscreen, CONF_TYPE_FLAG, 0, 0, 1},
	{"nofs", &fullscreen, CONF_TYPE_FLAG, 0, 1, 0},
	{"idx", &no_index, CONF_TYPE_FLAG, 0, 1, 0},
	{"noidx", &no_index, CONF_TYPE_FLAG, 0, 0, 1},
	{"v", &verbose, CONF_TYPE_INT, 0, 0, 0},
	{NULL, NULL, 0, 0, 0, 0}
};
