#pragma once

#include "video/out/gpu/ra.h"
#include "common.h"

struct vk_format {
    const char *name;
    VkFormat iformat;    // vulkan format enum
    int components;      // how many components are there
    int bytes;           // how many bytes is a texel
    int bits[4];         // how many bits per component
    enum ra_ctype ctype; // format representation type
    bool fucked_order;   // used for formats which are not simply rgba
};

extern const struct vk_format vk_formats[];
