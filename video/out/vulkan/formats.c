#include "formats.h"

const struct vk_format vk_formats[] = {
    // Regular, byte-aligned integer formats
    {"r8",       VK_FORMAT_R8_UNORM,                  1,  1,   {8             }, RA_CTYPE_UNORM },
    {"rg8",      VK_FORMAT_R8G8_UNORM,                2,  2,   {8,  8         }, RA_CTYPE_UNORM },
    {"rgb8",     VK_FORMAT_R8G8B8_UNORM,              3,  3,   {8,  8,  8     }, RA_CTYPE_UNORM },
    {"rgba8",    VK_FORMAT_R8G8B8A8_UNORM,            4,  4,   {8,  8,  8,  8 }, RA_CTYPE_UNORM },
    {"r16",      VK_FORMAT_R16_UNORM,                 1,  2,   {16            }, RA_CTYPE_UNORM },
    {"rg16",     VK_FORMAT_R16G16_UNORM,              2,  4,   {16, 16        }, RA_CTYPE_UNORM },
    {"rgb16",    VK_FORMAT_R16G16B16_UNORM,           3,  6,   {16, 16, 16    }, RA_CTYPE_UNORM },
    {"rgba16",   VK_FORMAT_R16G16B16A16_UNORM,        4,  8,   {16, 16, 16, 16}, RA_CTYPE_UNORM },

    // Special, integer-only formats
    {"r32ui",    VK_FORMAT_R32_UINT,                  1,  4,   {32            }, RA_CTYPE_UINT },
    {"rg32ui",   VK_FORMAT_R32G32_UINT,               2,  8,   {32, 32        }, RA_CTYPE_UINT },
    {"rgb32ui",  VK_FORMAT_R32G32B32_UINT,            3,  12,  {32, 32, 32    }, RA_CTYPE_UINT },
    {"rgba32ui", VK_FORMAT_R32G32B32A32_UINT,         4,  16,  {32, 32, 32, 32}, RA_CTYPE_UINT },
    {"r64ui",    VK_FORMAT_R64_UINT,                  1,  8,   {64            }, RA_CTYPE_UINT },
    {"rg64ui",   VK_FORMAT_R64G64_UINT,               2,  16,  {64, 64        }, RA_CTYPE_UINT },
    {"rgb64ui",  VK_FORMAT_R64G64B64_UINT,            3,  24,  {64, 64, 64    }, RA_CTYPE_UINT },
    {"rgba64ui", VK_FORMAT_R64G64B64A64_UINT,         4,  32,  {64, 64, 64, 64}, RA_CTYPE_UINT },

    // Packed integer formats
    {"rg4",      VK_FORMAT_R4G4_UNORM_PACK8,          2,  1,   {4,  4         }, RA_CTYPE_UNORM },
    {"rgba4",    VK_FORMAT_R4G4B4A4_UNORM_PACK16,     4,  2,   {4,  4,  4,  4 }, RA_CTYPE_UNORM },
    {"rgb565",   VK_FORMAT_R5G6B5_UNORM_PACK16,       3,  2,   {5,  6,  5     }, RA_CTYPE_UNORM },
    {"rgb5a1",   VK_FORMAT_R5G5B5A1_UNORM_PACK16,     4,  2,   {5,  5,  5,  1 }, RA_CTYPE_UNORM },

    // Float formats (native formats, hf = half float, df = double float)
    {"r16hf",    VK_FORMAT_R16_SFLOAT,                1,  2,   {16            }, RA_CTYPE_FLOAT },
    {"rg16hf",   VK_FORMAT_R16G16_SFLOAT,             2,  4,   {16, 16        }, RA_CTYPE_FLOAT },
    {"rgb16hf",  VK_FORMAT_R16G16B16_SFLOAT,          3,  6,   {16, 16, 16    }, RA_CTYPE_FLOAT },
    {"rgba16hf", VK_FORMAT_R16G16B16A16_SFLOAT,       4,  8,   {16, 16, 16, 16}, RA_CTYPE_FLOAT },
    {"r32f",     VK_FORMAT_R32_SFLOAT,                1,  4,   {32            }, RA_CTYPE_FLOAT },
    {"rg32f",    VK_FORMAT_R32G32_SFLOAT,             2,  8,   {32, 32        }, RA_CTYPE_FLOAT },
    {"rgb32f",   VK_FORMAT_R32G32B32_SFLOAT,          3, 12,   {32, 32, 32    }, RA_CTYPE_FLOAT },
    {"rgba32f",  VK_FORMAT_R32G32B32A32_SFLOAT,       4, 16,   {32, 32, 32, 32}, RA_CTYPE_FLOAT },
    {"r64df",    VK_FORMAT_R64_SFLOAT,                1,  8,   {64            }, RA_CTYPE_FLOAT },
    {"rg64df",   VK_FORMAT_R64G64_SFLOAT,             2, 16,   {64, 64        }, RA_CTYPE_FLOAT },
    {"rgb64df",  VK_FORMAT_R64G64B64_SFLOAT,          3, 24,   {64, 64, 64    }, RA_CTYPE_FLOAT },
    {"rgba64df", VK_FORMAT_R64G64B64A64_SFLOAT,       4, 32,   {64, 64, 64, 64}, RA_CTYPE_FLOAT },

    // "Swapped" component order images
    {"bgr8",     VK_FORMAT_B8G8R8_UNORM,              3,  3,   {8,  8,  8     }, RA_CTYPE_UNORM, true },
    {"bgra8",    VK_FORMAT_B8G8R8A8_UNORM,            4,  4,   {8,  8,  8,  8 }, RA_CTYPE_UNORM, true },
    {"bgra4",    VK_FORMAT_B4G4R4A4_UNORM_PACK16,     4,  2,   {4,  4,  4,  4 }, RA_CTYPE_UNORM, true },
    {"bgr565",   VK_FORMAT_B5G6R5_UNORM_PACK16,       3,  2,   {5,  6,  5     }, RA_CTYPE_UNORM, true },
    {"bgr5a1",   VK_FORMAT_B5G5R5A1_UNORM_PACK16,     4,  2,   {5,  5,  5,  1 }, RA_CTYPE_UNORM, true },
    {"a1rgb5",   VK_FORMAT_A1R5G5B5_UNORM_PACK16,     4,  2,   {1,  5,  5,  5 }, RA_CTYPE_UNORM, true },
    {"a2rgb10",  VK_FORMAT_A2R10G10B10_UNORM_PACK32,  4,  4,   {2,  10, 10, 10}, RA_CTYPE_UNORM, true },
    {"a2bgr10",  VK_FORMAT_A2B10G10R10_UNORM_PACK32,  4,  4,   {2,  10, 10, 10}, RA_CTYPE_UNORM, true },
    {"abgr8",    VK_FORMAT_A8B8G8R8_UNORM_PACK32,     4,  4,   {8,  8,  8,  8 }, RA_CTYPE_UNORM, true },
    {0}
};
