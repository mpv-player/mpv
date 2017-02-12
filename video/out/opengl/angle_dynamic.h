// Based on Khronos headers, thus MIT licensed.

#ifndef MP_ANGLE_DYNAMIC_H
#define MP_ANGLE_DYNAMIC_H

#include <stdbool.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#define ANGLE_FNS(FN) \
    FN(eglBindAPI, EGLBoolean (*EGLAPIENTRY PFN_eglBindAPI)(EGLenum)) \
    FN(eglBindTexImage, EGLBoolean (*EGLAPIENTRY PFN_eglBindTexImage) \
        (EGLDisplay, EGLSurface, EGLint)) \
    FN(eglChooseConfig, EGLBoolean (*EGLAPIENTRY PFN_eglChooseConfig) \
        (EGLDisplay, const EGLint *, EGLConfig *, EGLint, EGLint *)) \
    FN(eglCreateContext, EGLContext (*EGLAPIENTRY PFN_eglCreateContext) \
        (EGLDisplay, EGLConfig, EGLContext, const EGLint *)) \
    FN(eglCreatePbufferFromClientBuffer, EGLSurface (*EGLAPIENTRY \
        PFN_eglCreatePbufferFromClientBuffer)(EGLDisplay, EGLenum, \
        EGLClientBuffer, EGLConfig, const EGLint *)) \
    FN(eglCreateWindowSurface, EGLSurface (*EGLAPIENTRY \
        PFN_eglCreateWindowSurface)(EGLDisplay, EGLConfig, \
        EGLNativeWindowType, const EGLint *)) \
    FN(eglDestroyContext, EGLBoolean (*EGLAPIENTRY PFN_eglDestroyContext) \
        (EGLDisplay, EGLContext)) \
    FN(eglDestroySurface, EGLBoolean (*EGLAPIENTRY PFN_eglDestroySurface) \
        (EGLDisplay, EGLSurface)) \
    FN(eglGetConfigAttrib, EGLBoolean (*EGLAPIENTRY PFN_eglGetConfigAttrib) \
        (EGLDisplay, EGLConfig, EGLint, EGLint *)) \
    FN(eglGetCurrentContext, EGLContext (*EGLAPIENTRY \
        PFN_eglGetCurrentContext)(void)) \
    FN(eglGetCurrentDisplay, EGLDisplay (*EGLAPIENTRY \
        PFN_eglGetCurrentDisplay)(void)) \
    FN(eglGetDisplay, EGLDisplay (*EGLAPIENTRY PFN_eglGetDisplay) \
        (EGLNativeDisplayType)) \
    FN(eglGetError, EGLint (*EGLAPIENTRY PFN_eglGetError)(void)) \
    FN(eglGetProcAddress, void *(*EGLAPIENTRY \
        PFN_eglGetProcAddress)(const char *)) \
    FN(eglInitialize, EGLBoolean (*EGLAPIENTRY PFN_eglInitialize) \
        (EGLDisplay, EGLint *, EGLint *)) \
    FN(eglMakeCurrent, EGLBoolean (*EGLAPIENTRY PFN_eglMakeCurrent) \
        (EGLDisplay, EGLSurface, EGLSurface, EGLContext)) \
    FN(eglQueryString, const char *(*EGLAPIENTRY PFN_eglQueryString) \
        (EGLDisplay, EGLint)) \
    FN(eglSwapBuffers, EGLBoolean (*EGLAPIENTRY PFN_eglSwapBuffers) \
        (EGLDisplay, EGLSurface)) \
    FN(eglSwapInterval, EGLBoolean (*EGLAPIENTRY PFN_eglSwapInterval) \
        (EGLDisplay, EGLint)) \
    FN(eglReleaseTexImage, EGLBoolean (*EGLAPIENTRY PFN_eglReleaseTexImage) \
        (EGLDisplay, EGLSurface, EGLint)) \
    FN(eglTerminate, EGLBoolean (*EGLAPIENTRY PFN_eglTerminate)(EGLDisplay)) \
    FN(eglWaitClient, EGLBoolean (*EGLAPIENTRY PFN_eglWaitClient)(void))

#define ANGLE_EXT_DECL(NAME, VAR) \
    extern VAR;
ANGLE_FNS(ANGLE_EXT_DECL)

bool angle_load(void);

// Source compatibility to statically linked ANGLE.
#ifndef ANGLE_NO_ALIASES
#define eglBindAPI                      PFN_eglBindAPI
#define eglBindTexImage                 PFN_eglBindTexImage
#define eglChooseConfig                 PFN_eglChooseConfig
#define eglCreateContext                PFN_eglCreateContext
#define eglCreatePbufferFromClientBuffer PFN_eglCreatePbufferFromClientBuffer
#define eglCreateWindowSurface          PFN_eglCreateWindowSurface
#define eglDestroyContext               PFN_eglDestroyContext
#define eglDestroySurface               PFN_eglDestroySurface
#define eglGetConfigAttrib              PFN_eglGetConfigAttrib
#define eglGetCurrentContext            PFN_eglGetCurrentContext
#define eglGetCurrentDisplay            PFN_eglGetCurrentDisplay
#define eglGetDisplay                   PFN_eglGetDisplay
#define eglGetError                     PFN_eglGetError
#define eglGetProcAddress               PFN_eglGetProcAddress
#define eglInitialize                   PFN_eglInitialize
#define eglMakeCurrent                  PFN_eglMakeCurrent
#define eglQueryString                  PFN_eglQueryString
#define eglReleaseTexImage              PFN_eglReleaseTexImage
#define eglSwapBuffers                  PFN_eglSwapBuffers
#define eglSwapInterval                 PFN_eglSwapInterval
#define eglTerminate                    PFN_eglTerminate
#define eglWaitClient                   PFN_eglWaitClient
#endif

#endif
