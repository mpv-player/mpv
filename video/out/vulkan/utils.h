#pragma once
#include "common.h"
#include "video/out/gpu/context.h"

bool mpvk_init(struct mpvk_ctx *vk, struct ra_ctx *ctx, const char *surface_ext);
void mpvk_uninit(struct mpvk_ctx *vk);
