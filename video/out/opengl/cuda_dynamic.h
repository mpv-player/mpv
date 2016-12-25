/*
 * This file is part of mpv.
 *
 * It is based on an equivalent file in ffmpeg that was
 * constructed from documentation, rather than from any
 * original cuda headers.
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

#ifndef MPV_CUDA_DYNAMIC_H
#define MPV_CUDA_DYNAMIC_H

#include <stdbool.h>
#include <stddef.h>

#define CUDA_VERSION 7050

#if defined(_WIN32) || defined(__CYGWIN__)
#define CUDAAPI __stdcall
#else
#define CUDAAPI
#endif

#define CU_CTX_SCHED_BLOCKING_SYNC 4

typedef int CUdevice;

typedef struct CUarray_st *CUarray;
typedef struct CUgraphicsResource_st *CUgraphicsResource;
typedef struct CUstream_st *CUstream;

typedef void* CUcontext;
#if defined(__x86_64) || defined(AMD64) || defined(_M_AMD64)
typedef unsigned long long CUdeviceptr;
#else
typedef unsigned int CUdeviceptr;
#endif

typedef enum cudaError_enum {
    CUDA_SUCCESS = 0
} CUresult;

typedef enum CUmemorytype_enum {
    CU_MEMORYTYPE_HOST = 1,
    CU_MEMORYTYPE_DEVICE = 2,
    CU_MEMORYTYPE_ARRAY = 3
} CUmemorytype;

typedef struct CUDA_MEMCPY2D_st {
    size_t srcXInBytes;
    size_t srcY;
    CUmemorytype srcMemoryType;
    const void *srcHost;
    CUdeviceptr srcDevice;
    CUarray srcArray;
    size_t srcPitch;

    size_t dstXInBytes;
    size_t dstY;
    CUmemorytype dstMemoryType;
    void *dstHost;
    CUdeviceptr dstDevice;
    CUarray dstArray;
    size_t dstPitch;

    size_t WidthInBytes;
    size_t Height;
} CUDA_MEMCPY2D;

typedef enum CUGLDeviceList_enum {
    CU_GL_DEVICE_LIST_ALL = 1,
    CU_GL_DEVICE_LIST_CURRENT_FRAME = 2,
    CU_GL_DEVICE_LIST_NEXT_FRAME = 3,
} CUGLDeviceList;

typedef unsigned int    GLenum;
typedef unsigned int    GLuint;

#define CU_GRAPHICS_REGISTER_FLAGS_WRITE_DISCARD 2

typedef CUresult CUDAAPI tcuInit(unsigned int Flags);
typedef CUresult CUDAAPI tcuCtxCreate_v2(CUcontext *pctx, unsigned int flags, CUdevice dev);
typedef CUresult CUDAAPI tcuCtxPushCurrent_v2(CUcontext *pctx);
typedef CUresult CUDAAPI tcuCtxPopCurrent_v2(CUcontext *pctx);
typedef CUresult CUDAAPI tcuCtxDestroy_v2(CUcontext ctx);
typedef CUresult CUDAAPI tcuMemcpy2D_v2(const CUDA_MEMCPY2D *pcopy);
typedef CUresult CUDAAPI tcuGetErrorName(CUresult error, const char** pstr);
typedef CUresult CUDAAPI tcuGetErrorString(CUresult error, const char** pstr);
typedef CUresult CUDAAPI tcuGLGetDevices_v2(unsigned int* pCudaDeviceCount, CUdevice* pCudaDevices, unsigned int cudaDeviceCount, CUGLDeviceList deviceList);
typedef CUresult CUDAAPI tcuGraphicsGLRegisterImage(CUgraphicsResource* pCudaResource, GLuint image, GLenum target, unsigned int Flags);
typedef CUresult CUDAAPI tcuGraphicsUnregisterResource(CUgraphicsResource resource);
typedef CUresult CUDAAPI tcuGraphicsMapResources(unsigned int count, CUgraphicsResource* resources, CUstream hStream);
typedef CUresult CUDAAPI tcuGraphicsUnmapResources(unsigned int count, CUgraphicsResource* resources, CUstream hStream);
typedef CUresult CUDAAPI tcuGraphicsSubResourceGetMappedArray(CUarray* pArray, CUgraphicsResource resource, unsigned int arrayIndex, unsigned int mipLevel);

#define CUDA_FNS(FN) \
    FN(cuInit, tcuInit) \
    FN(cuCtxCreate_v2, tcuCtxCreate_v2) \
    FN(cuCtxPushCurrent_v2, tcuCtxPushCurrent_v2) \
    FN(cuCtxPopCurrent_v2, tcuCtxPopCurrent_v2) \
    FN(cuCtxDestroy_v2, tcuCtxDestroy_v2) \
    FN(cuMemcpy2D_v2, tcuMemcpy2D_v2) \
    FN(cuGetErrorName, tcuGetErrorName) \
    FN(cuGetErrorString, tcuGetErrorString) \
    FN(cuGLGetDevices_v2, tcuGLGetDevices_v2) \
    FN(cuGraphicsGLRegisterImage, tcuGraphicsGLRegisterImage) \
    FN(cuGraphicsUnregisterResource, tcuGraphicsUnregisterResource) \
    FN(cuGraphicsMapResources, tcuGraphicsMapResources) \
    FN(cuGraphicsUnmapResources, tcuGraphicsUnmapResources) \
    FN(cuGraphicsSubResourceGetMappedArray, tcuGraphicsSubResourceGetMappedArray) \

#define CUDA_EXT_DECL(NAME, TYPE) \
    extern TYPE *mpv_ ## NAME;

CUDA_FNS(CUDA_EXT_DECL)

#define cuInit mpv_cuInit
#define cuCtxCreate mpv_cuCtxCreate_v2
#define cuCtxPushCurrent mpv_cuCtxPushCurrent_v2
#define cuCtxPopCurrent mpv_cuCtxPopCurrent_v2
#define cuCtxDestroy mpv_cuCtxDestroy_v2
#define cuMemcpy2D mpv_cuMemcpy2D_v2
#define cuGetErrorName mpv_cuGetErrorName
#define cuGetErrorString mpv_cuGetErrorString
#define cuGLGetDevices mpv_cuGLGetDevices_v2
#define cuGraphicsGLRegisterImage mpv_cuGraphicsGLRegisterImage
#define cuGraphicsUnregisterResource mpv_cuGraphicsUnregisterResource
#define cuGraphicsMapResources mpv_cuGraphicsMapResources
#define cuGraphicsUnmapResources mpv_cuGraphicsUnmapResources
#define cuGraphicsSubResourceGetMappedArray mpv_cuGraphicsSubResourceGetMappedArray

bool cuda_load(void);

#endif // MPV_CUDA_DYNAMIC_H
