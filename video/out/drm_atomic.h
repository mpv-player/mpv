/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MP_DRMATOMIC_H
#define MP_DRMATOMIC_H

#include <stdlib.h>
#include <stdbool.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include "common/msg.h"
#include "video/csputils.h"

#define DRM_OPTS_PRIMARY_PLANE -1
#define DRM_OPTS_OVERLAY_PLANE -2

struct drm_mode {
    drmModeModeInfo mode;
    uint32_t blob_id;
};

struct drm_atomic_plane_state {
    uint64_t fb_id;
    uint64_t crtc_id;
    uint64_t src_x;
    uint64_t src_y;
    uint64_t src_w;
    uint64_t src_h;
    uint64_t crtc_x;
    uint64_t crtc_y;
    uint64_t crtc_w;
    uint64_t crtc_h;
    uint64_t zpos;
};

// Used to store the restore state for VT switching and uninit
struct drm_atomic_state {
    bool saved;
    struct {
        uint64_t crtc_id;
    } connector;
    struct {
        struct drm_mode mode;
        uint64_t active;
    } crtc;
    struct drm_atomic_plane_state draw_plane;
    struct drm_atomic_plane_state drmprime_video_plane;
};

struct drm_object {
    int fd;
    uint32_t id;
    uint32_t type;
    drmModeObjectProperties *props;
    drmModePropertyRes **props_info;
};

#ifndef HAVE_HDR_OUTPUT_METADATA
#define DRM_HAS_HDR_METADATA_INFOFFRAME
#define DRM_HDMI_STATIC_METADATA_TYPE1 1
/**
 * struct hdr_metadata_infoframe - HDR Metadata Infoframe Data.
 *
 * HDR Metadata Infoframe as per CTA 861.G spec. This is expected
 * to match exactly with the spec.
 *
 * Userspace is expected to pass the metadata information as per
 * the format described in this structure.
 */
struct hdr_metadata_infoframe {
	/**
	 * @eotf: Electro-Optical Transfer Function (EOTF)
	 * used in the stream.
	 */
	__u8 eotf;
	/**
	 * @metadata_type: Static_Metadata_Descriptor_ID.
	 */
	__u8 metadata_type;
	/**
	 * @display_primaries: Color Primaries of the Data.
	 * These are coded as unsigned 16-bit values in units of
	 * 0.00002, where 0x0000 represents zero and 0xC350
	 * represents 1.0000.
	 * @display_primaries.x: X cordinate of color primary.
	 * @display_primaries.y: Y cordinate of color primary.
	 */
	struct {
		__u16 x, y;
		} display_primaries[3];
	/**
	 * @white_point: White Point of Colorspace Data.
	 * These are coded as unsigned 16-bit values in units of
	 * 0.00002, where 0x0000 represents zero and 0xC350
	 * represents 1.0000.
	 * @white_point.x: X cordinate of whitepoint of color primary.
	 * @white_point.y: Y cordinate of whitepoint of color primary.
	 */
	struct {
		__u16 x, y;
		} white_point;
	/**
	 * @max_display_mastering_luminance: Max Mastering Display Luminance.
	 * This value is coded as an unsigned 16-bit value in units of 1 cd/m2,
	 * where 0x0001 represents 1 cd/m2 and 0xFFFF represents 65535 cd/m2.
	 */
	__u16 max_display_mastering_luminance;
	/**
	 * @min_display_mastering_luminance: Min Mastering Display Luminance.
	 * This value is coded as an unsigned 16-bit value in units of
	 * 0.0001 cd/m2, where 0x0001 represents 0.0001 cd/m2 and 0xFFFF
	 * represents 6.5535 cd/m2.
	 */
	__u16 min_display_mastering_luminance;
	/**
	 * @max_cll: Max Content Light Level.
	 * This value is coded as an unsigned 16-bit value in units of 1 cd/m2,
	 * where 0x0001 represents 1 cd/m2 and 0xFFFF represents 65535 cd/m2.
	 */
	__u16 max_cll;
	/**
	 * @max_fall: Max Frame Average Light Level.
	 * This value is coded as an unsigned 16-bit value in units of 1 cd/m2,
	 * where 0x0001 represents 1 cd/m2 and 0xFFFF represents 65535 cd/m2.
	 */
	__u16 max_fall;
};

/**
 * struct hdr_output_metadata - HDR output metadata
 *
 * Metadata Information to be passed from userspace
 */
struct hdr_output_metadata {
	/**
	 * @metadata_type: Static_Metadata_Descriptor_ID.
	 */
	__u32 metadata_type;
	/**
	 * @hdmi_metadata_type1: HDR Metadata Infoframe.
	 */
	union {
		struct hdr_metadata_infoframe hdmi_metadata_type1;
	};
};

#endif

struct drm_hdr_metadata {
#ifdef DRM_HAS_HDR_METADATA_INFOFFRAME
    struct hdr_output_metadata data;
    uint32_t blob_id;
#endif
};


struct drm_atomic_context {
    int fd;

    struct drm_object *crtc;
    struct drm_object *connector;
    struct drm_object *draw_plane;
    struct drm_object *drmprime_video_plane;

    drmModeAtomicReq *request;

    struct drm_atomic_state old_state;
    
    struct drm_hdr_metadata hdr_metadata;
};



int drm_object_create_properties(struct mp_log *log, int fd, struct drm_object *object);
void drm_object_free_properties(struct drm_object *object);
int drm_object_get_property(struct drm_object *object, char *name, uint64_t *value);
int drm_object_set_property(drmModeAtomicReq *request, struct drm_object *object, char *name, uint64_t value);
drmModePropertyBlobPtr drm_object_get_property_blob(struct drm_object *object, char *name);
struct drm_object * drm_object_create(struct mp_log *log, int fd, uint32_t object_id, uint32_t type);
void drm_object_free(struct drm_object *object);
void drm_object_print_info(struct mp_log *log, struct drm_object *object);
struct drm_atomic_context *drm_atomic_create_context(struct mp_log *log, int fd, int crtc_id, int connector_id,
                                                     int draw_plane_idx, int drmprime_video_plane_idx);
void drm_atomic_destroy_context(struct drm_atomic_context *ctx);

bool drm_atomic_save_old_state(struct drm_atomic_context *ctx);
bool drm_atomic_restore_old_state(drmModeAtomicReq *request, struct drm_atomic_context *ctx);

bool drm_mode_ensure_blob(int fd, struct drm_mode *mode);
bool drm_mode_destroy_blob(int fd, struct drm_mode *mode);

// append HDR blob to the connector properties
void drm_send_hdrmeta(struct drm_atomic_context *ctx, struct mp_colorspace *color);
void drm_destroy_hdrmeta(struct drm_atomic_context *ctx);


#endif // MP_DRMATOMIC_H
