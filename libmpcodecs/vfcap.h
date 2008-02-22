// VFCAP_* values: they are flags, returned by query_format():

#ifndef MPLAYER_VFCAP_H
#define MPLAYER_VFCAP_H

// set, if the given colorspace is supported (with or without conversion)
#define VFCAP_CSP_SUPPORTED 0x1
// set, if the given colorspace is supported _without_ conversion
#define VFCAP_CSP_SUPPORTED_BY_HW 0x2
// set if the driver/filter can draw OSD
#define VFCAP_OSD 0x4
// set if the driver/filter can handle compressed SPU stream
#define VFCAP_SPU 0x8
// scaling up/down by hardware, or software:
#define VFCAP_HWSCALE_UP 0x10
#define VFCAP_HWSCALE_DOWN 0x20
#define VFCAP_SWSCALE 0x40
// driver/filter can do vertical flip (upside-down)
#define VFCAP_FLIP 0x80

// driver/hardware handles timing (blocking)
#define VFCAP_TIMER 0x100
// driver _always_ flip image upside-down (for ve_vfw)
#define VFCAP_FLIPPED 0x200
// vf filter: accepts stride (put_image)
// vo driver: has draw_slice() support for the given csp
#define VFCAP_ACCEPT_STRIDE 0x400
// filter does postprocessing (so you shouldn't scale/filter image before it)
#define VFCAP_POSTPROC 0x800
// filter cannot be reconfigured to different size & format
#define VFCAP_CONSTANT 0x1000
// filter can draw EOSD
#define VFCAP_EOSD 0x2000
// filter will draw EOSD at screen resolution (without scaling)
#define VFCAP_EOSD_UNSCALED 0x4000

#endif /* MPLAYER_VFCAP_H */
