// VFCAP_* values: they are flags, returned by query_format():

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
// driver accept stride: (put_image/draw_frame)
#define VFCAP_ACCEPT_STRIDE 0x400

#define VFCAP_POSTPROC 0x800

