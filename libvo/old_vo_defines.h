#ifndef MPLAYER_OLD_VO_DEFINES_H
#define MPLAYER_OLD_VO_DEFINES_H

#include "options.h"
#include "video_out.h"
#include "old_vo_wrapper.h"

// Triggers more defines in x11_common.h
#define IS_OLD_VO 1

#define vo_ontop global_vo->opts->vo_ontop
#define vo_config_count global_vo->config_count
#define vo_dx global_vo->dx
#define vo_dy global_vo->dy
#define vo_dwidth global_vo->dwidth
#define vo_dheight global_vo->dheight

#endif
