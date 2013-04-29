/*
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * You can alternatively redistribute this file and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#ifdef MP_GET_GL_WORKAROUNDS

// workaround for some gl.h headers
#ifndef GLAPIENTRY
#ifdef APIENTRY
#define GLAPIENTRY APIENTRY
#elif defined(CONFIG_GL_WIN32)
#define GLAPIENTRY __stdcall
#else
#define GLAPIENTRY
#endif
#endif

/**
 * \defgroup glextdefines OpenGL extension defines
 *
 * conditionally define all extension defines used.
 * vendor specific extensions should be marked as such
 * (e.g. _NV), _ARB is not used to ease readability.
 * \{
 */
#ifndef GL_TEXTURE_3D
#define GL_TEXTURE_3D 0x806F
#endif
#ifndef GL_TEXTURE_WRAP_R
#define GL_TEXTURE_WRAP_R 0x8072
#endif
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#ifndef GL_GENERATE_MIPMAP
#define GL_GENERATE_MIPMAP 0x8191
#endif
#ifndef GL_TEXT_FRAGMENT_SHADER_ATI
#define GL_TEXT_FRAGMENT_SHADER_ATI 0x8200
#endif
#ifndef GL_FRAGMENT_SHADER_ATI
#define GL_FRAGMENT_SHADER_ATI 0x8920
#endif
#ifndef GL_NUM_FRAGMENT_REGISTERS_ATI
#define GL_NUM_FRAGMENT_REGISTERS_ATI 0x896E
#endif
#ifndef GL_REG_0_ATI
#define GL_REG_0_ATI 0x8921
#endif
#ifndef GL_REG_1_ATI
#define GL_REG_1_ATI 0x8922
#endif
#ifndef GL_REG_2_ATI
#define GL_REG_2_ATI 0x8923
#endif
#ifndef GL_CON_0_ATI
#define GL_CON_0_ATI 0x8941
#endif
#ifndef GL_CON_1_ATI
#define GL_CON_1_ATI 0x8942
#endif
#ifndef GL_CON_2_ATI
#define GL_CON_2_ATI 0x8943
#endif
#ifndef GL_CON_3_ATI
#define GL_CON_3_ATI 0x8944
#endif
#ifndef GL_ADD_ATI
#define GL_ADD_ATI 0x8963
#endif
#ifndef GL_MUL_ATI
#define GL_MUL_ATI 0x8964
#endif
#ifndef GL_MAD_ATI
#define GL_MAD_ATI 0x8968
#endif
#ifndef GL_SWIZZLE_STR_ATI
#define GL_SWIZZLE_STR_ATI 0x8976
#endif
#ifndef GL_4X_BIT_ATI
#define GL_4X_BIT_ATI 2
#endif
#ifndef GL_8X_BIT_ATI
#define GL_8X_BIT_ATI 4
#endif
#ifndef GL_BIAS_BIT_ATI
#define GL_BIAS_BIT_ATI 8
#endif
#ifndef GL_MAX_TEXTURE_UNITS
#define GL_MAX_TEXTURE_UNITS 0x84E2
#endif
#ifndef GL_TEXTURE0
#define GL_TEXTURE0 0x84C0
#endif
#ifndef GL_TEXTURE1
#define GL_TEXTURE1 0x84C1
#endif
#ifndef GL_TEXTURE2
#define GL_TEXTURE2 0x84C2
#endif
#ifndef GL_TEXTURE3
#define GL_TEXTURE3 0x84C3
#endif
#ifndef GL_TEXTURE_RECTANGLE
#define GL_TEXTURE_RECTANGLE 0x84F5
#endif
#ifndef GL_PIXEL_UNPACK_BUFFER
#define GL_PIXEL_UNPACK_BUFFER 0x88EC
#endif
#ifndef GL_STREAM_DRAW
#define GL_STREAM_DRAW 0x88E0
#endif
#ifndef GL_DYNAMIC_DRAW
#define GL_DYNAMIC_DRAW 0x88E8
#endif
#ifndef GL_WRITE_ONLY
#define GL_WRITE_ONLY 0x88B9
#endif
#ifndef GL_BGR
#define GL_BGR 0x80E0
#endif
#ifndef GL_BGRA
#define GL_BGRA 0x80E1
#endif
#ifndef GL_UNSIGNED_BYTE_3_3_2
#define GL_UNSIGNED_BYTE_3_3_2 0x8032
#endif
#ifndef GL_UNSIGNED_BYTE_2_3_3_REV
#define GL_UNSIGNED_BYTE_2_3_3_REV 0x8362
#endif
#ifndef GL_UNSIGNED_SHORT_4_4_4_4
#define GL_UNSIGNED_SHORT_4_4_4_4 0x8033
#endif
#ifndef GL_UNSIGNED_SHORT_4_4_4_4_REV
#define GL_UNSIGNED_SHORT_4_4_4_4_REV 0x8365
#endif
#ifndef GL_UNSIGNED_SHORT_5_6_5
#define GL_UNSIGNED_SHORT_5_6_5 0x8363
#endif
#ifndef GL_UNSIGNED_INT_8_8_8_8
#define GL_UNSIGNED_INT_8_8_8_8 0x8035
#endif
#ifndef GL_UNSIGNED_INT_8_8_8_8_REV
#define GL_UNSIGNED_INT_8_8_8_8_REV 0x8367
#endif
#ifndef GL_UNSIGNED_SHORT_5_6_5_REV
#define GL_UNSIGNED_SHORT_5_6_5_REV 0x8364
#endif
#ifndef GL_UNSIGNED_INT_10_10_10_2
#define GL_UNSIGNED_INT_10_10_10_2 0x8036
#endif
#ifndef GL_UNSIGNED_INT_2_10_10_10_REV
#define GL_UNSIGNED_INT_2_10_10_10_REV 0x8368
#endif
#ifndef GL_UNSIGNED_SHORT_5_5_5_1
#define GL_UNSIGNED_SHORT_5_5_5_1 0x8034
#endif
#ifndef GL_UNSIGNED_SHORT_1_5_5_5_REV
#define GL_UNSIGNED_SHORT_1_5_5_5_REV 0x8366
#endif
#ifndef GL_UNSIGNED_SHORT_8_8
#define GL_UNSIGNED_SHORT_8_8 0x85BA
#endif
#ifndef GL_UNSIGNED_SHORT_8_8_REV
#define GL_UNSIGNED_SHORT_8_8_REV 0x85BB
#endif
#ifndef GL_YCBCR_MESA
#define GL_YCBCR_MESA 0x8757
#endif
#ifndef GL_RGB32F
#define GL_RGB32F 0x8815
#endif
#ifndef GL_FLOAT_RGB32_NV
#define GL_FLOAT_RGB32_NV 0x8889
#endif
#ifndef GL_LUMINANCE16
#define GL_LUMINANCE16 0x8042
#endif
#ifndef GL_R16
#define GL_R16 0x822A
#endif
#ifndef GL_UNPACK_CLIENT_STORAGE_APPLE
#define GL_UNPACK_CLIENT_STORAGE_APPLE 0x85B2
#endif
#ifndef GL_FRAGMENT_PROGRAM
#define GL_FRAGMENT_PROGRAM 0x8804
#endif
#ifndef GL_PROGRAM_FORMAT_ASCII
#define GL_PROGRAM_FORMAT_ASCII 0x8875
#endif
#ifndef GL_PROGRAM_ERROR_POSITION
#define GL_PROGRAM_ERROR_POSITION 0x864B
#endif
#ifndef GL_MAX_TEXTURE_IMAGE_UNITS
#define GL_MAX_TEXTURE_IMAGE_UNITS 0x8872
#endif
#ifndef GL_PROGRAM_ERROR_STRING
#define GL_PROGRAM_ERROR_STRING 0x8874
#endif
#ifndef GL_RGBA32F
#define GL_RGBA32F 0x8814
#endif
/** \} */ // end of glextdefines group


#if defined(CONFIG_GL_WIN32) && !defined(WGL_CONTEXT_MAJOR_VERSION_ARB)
/* these are supposed to be defined in wingdi.h but mingw's is too old */
/* only the bits actually used by mplayer are defined */
/* reference: http://www.opengl.org/registry/specs/ARB/wgl_create_context.txt */

#define WGL_CONTEXT_MAJOR_VERSION_ARB          0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB          0x2092
#define WGL_CONTEXT_FLAGS_ARB                  0x2094
#define WGL_CONTEXT_PROFILE_MASK_ARB           0x9126
#define WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB 0x0002
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB   0x00000001
#endif

// Define just enough constants to make the OpenGL 3 code compile against
// older SDKs. Values are taken straight from OpenGL/gl3.h
#if defined __APPLE__ && !(defined GL_VERSION_3_0)
#define GL_RGBA16F                        0x881A
#define GL_RGB16F                         0x881B
#define GL_MAJOR_VERSION                  0x821B
#define GL_MINOR_VERSION                  0x821C
#define GL_NUM_EXTENSIONS                 0x821D

#ifndef GL_ARB_framebuffer_sRGB
#define GL_FRAMEBUFFER_SRGB               0x8DB9
#endif
#endif

#undef MP_GET_GL_WORKAROUNDS

#endif

#ifdef MP_GET_GLX_WORKAROUNDS

// FreeBSD 10.0-CURRENT lacks the GLX_ARB_create_context extension completely
#ifndef GLX_CONTEXT_MAJOR_VERSION_ARB
#define GLX_CONTEXT_MAJOR_VERSION_ARB           0x2091
#define GLX_CONTEXT_MINOR_VERSION_ARB           0x2092
#define GLX_CONTEXT_FLAGS_ARB                   0x2094
#define GLX_CONTEXT_PROFILE_MASK_ARB            0x9126
#ifndef __APPLE__
// These are respectively 0x00000001 and 0x00000002 on OSX
#define GLX_CONTEXT_DEBUG_BIT_ARB               0x0001
#define GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB  0x0002
#endif
#define GLX_CONTEXT_CORE_PROFILE_BIT_ARB        0x00000001
#define GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB 0x00000002
#endif

#undef MP_GET_GLX_WORKAROUNDS

#endif
