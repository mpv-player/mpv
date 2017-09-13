#pragma once

#include "video/out/gpu/context.h"
#include "common.h"

// Helpers for ra_ctx based on ra_vk. These initialize ctx->ra and ctx->swchain.
void ra_vk_ctx_uninit(struct ra_ctx *ctx);
bool ra_vk_ctx_init(struct ra_ctx *ctx, struct mpvk_ctx *vk,
                    VkPresentModeKHR preferred_mode);
bool ra_vk_ctx_resize(struct ra_swapchain *sw, int w, int h);

// May be called on a ra_ctx of any type.
struct mpvk_ctx *ra_vk_ctx_get(struct ra_ctx *ctx);
