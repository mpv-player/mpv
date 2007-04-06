#ifndef _DHAHELPER_H
#define _DHAHELPER_H 1

// Define the various device type values.  Note that values used by Microsoft
// Corporation are in the range 0-32767, and 32768-65535 are reserved for use
// by customers.

#define FILE_DEVICE_DHAHELPER 0x00008011

// Macro definition for defining IOCTL and FSCTL function control codes.
// Note that function codes 0-2047 are reserved for Microsoft Corporation,
// and 2048-4095 are reserved for customers.

#define DHAHELPER_IOCTL_INDEX 0x810

#define IOCTL_DHAHELPER_MAPPHYSTOLIN     CTL_CODE(FILE_DEVICE_DHAHELPER,  \
                                     DHAHELPER_IOCTL_INDEX,      \
                                     METHOD_BUFFERED,        \
                                     FILE_ANY_ACCESS)

#define IOCTL_DHAHELPER_UNMAPPHYSADDR    CTL_CODE(FILE_DEVICE_DHAHELPER,  \
                                     DHAHELPER_IOCTL_INDEX + 1,  \
                                     METHOD_BUFFERED,        \
                                     FILE_ANY_ACCESS)

#define IOCTL_DHAHELPER_ENABLEDIRECTIO   CTL_CODE(FILE_DEVICE_DHAHELPER,  \
                                     DHAHELPER_IOCTL_INDEX + 2,   \
                                     METHOD_BUFFERED,         \
                                     FILE_ANY_ACCESS)

#define IOCTL_DHAHELPER_DISABLEDIRECTIO  CTL_CODE(FILE_DEVICE_DHAHELPER,  \
                                     DHAHELPER_IOCTL_INDEX + 3,   \
                                     METHOD_BUFFERED,         \
                                     FILE_ANY_ACCESS)
         
         
#if !defined(__MINGW32__) && !defined(__CYGWIN__) 
#pragma pack(1)
typedef struct dhahelper_t {
#else
struct __attribute__((__packed__)) dhahelper_t {
#endif
  unsigned int size;
  void* base;
  void* ptr;
};

typedef struct dhahelper_t dhahelper_t;

#endif
