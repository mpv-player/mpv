/*
 * PROJECT:         ReactOS
 * LICENSE:         GPL - See COPYING in the top level directory
 * FILE:            include/psdk/ntverp.h
 * PURPOSE:         Master Version File.
 *                  This file should be modified only by the official builder
 *                  to update VERSION, VER_PRODUCTVERSION, VER_PRODUCTVERSION_
 *                  STR and VER_PRODUCTBETA_STR values.
 *                  The VER_PRODUCTBUILD lines must contain the product
 *                  comments and end with the build#<CR><LF>.
 *                  The VER_PRODUCTBETA_STR lines must contain the product
 *                  comments and end with "somestring"<CR><LF.
 * PROGRAMMERS:     Alex Ionescu (alex.ionescu@reactos.org)
 */

#ifndef MPLAYER_NTVERP_H
#define MPLAYER_NTVERP_H

//
// Windows NT Build 3790.1830
//
#define VER_PRODUCTBUILD                    3790
#define VER_PRODUCTBUILD_QFE                1830

//
// Windows NT Version 5.2
//
#define VER_PRODUCTMAJORVERSION             5
#define VER_PRODUCTMINORVERSION             2
#define VER_PRODUCTVERSION_W                (0x0502)
#define VER_PRODUCTVERSION_DW               (0x05020000 | VER_PRODUCTBUILD)

//
// Not a beta
//
#define VER_PRODUCTBETA_STR                 ""

//
// ANSI String Generating Macros
//
#define VER_PRODUCTVERSION_MAJORMINOR2(x,y) \
    #x "." #y
#define VER_PRODUCTVERSION_MAJORMINOR1(x,y) \
    VER_PRODUCTVERSION_MAJORMINOR2(x, y)
#define VER_PRODUCTVERSION_STRING           \
    VER_PRODUCTVERSION_MAJORMINOR1(VER_PRODUCTMAJORVERSION, VER_PRODUCTMINORVERSION)

//
// Unicode String Generating Macros
//
#define LVER_PRODUCTVERSION_MAJORMINOR2(x,y)\
    L#x L"." L#y
#define LVER_PRODUCTVERSION_MAJORMINOR1(x,y)\
    LVER_PRODUCTVERSION_MAJORMINOR2(x, y)
#define LVER_PRODUCTVERSION_STRING          \
    LVER_PRODUCTVERSION_MAJORMINOR1(VER_PRODUCTMAJORVERSION, VER_PRODUCTMINORVERSION)

//
// Full Product Version
//
#define VER_PRODUCTVERSION                  \
    VER_PRODUCTMAJORVERSION,VER_PRODUCTMINORVERSION,VER_PRODUCTBUILD,VER_PRODUCTBUILD_QFE

//
// Padding for ANSI Version String
//
#if     (VER_PRODUCTBUILD < 10)
#define VER_BPAD "000"
#elif   (VER_PRODUCTBUILD < 100)
#define VER_BPAD "00"
#elif   (VER_PRODUCTBUILD < 1000)
#define VER_BPAD "0"
#else
#define VER_BPAD
#endif

//
// Padding for Unicode Version String
//
#if     (VER_PRODUCTBUILD < 10)
#define LVER_BPAD L"000"
#elif   (VER_PRODUCTBUILD < 100)
#define LVER_BPAD L"00"
#elif   (VER_PRODUCTBUILD < 1000)
#define LVER_BPAD L"0"
#else
#define LVER_BPAD
#endif

//
// ANSI Product Version String
//
#define VER_PRODUCTVERSION_STR2(x,y)        \
    VER_PRODUCTVERSION_STRING "." VER_BPAD #x "." #y
#define VER_PRODUCTVERSION_STR1(x,y)        \
    VER_PRODUCTVERSION_STR2(x, y)
#define VER_PRODUCTVERSION_STR              \
    VER_PRODUCTVERSION_STR1(VER_PRODUCTBUILD, VER_PRODUCTBUILD_QFE)

//
// Unicode Product Version String
//
#define LVER_PRODUCTVERSION_STR2(x,y)       \
    LVER_PRODUCTVERSION_STRING L"." LVER_BPAD L#x L"." L#y
#define LVER_PRODUCTVERSION_STR1(x,y)       \
    LVER_PRODUCTVERSION_STR2(x, y)
#define LVER_PRODUCTVERSION_STR             \
    LVER_PRODUCTVERSION_STR1(VER_PRODUCTBUILD, VER_PRODUCTBUILD_QFE)

//
// Debug Flag
//
#if DBG
#define VER_DEBUG                           VS_FF_DEBUG
#else
#define VER_DEBUG                           0
#endif

//
// Beta Flag
//
#if BETA
#define VER_PRERELEASE                      VS_FF_PRERELEASE
#else
#define VER_PRERELEASE                      0
#endif

//
// Internal Flag
//
#if OFFICIAL_BUILD
#define VER_PRIVATE                         0
#else
#define VER_PRIVATE                         VS_FF_PRIVATEBUILD
#endif

//
// Other Flags
//
#define VER_FILEFLAGSMASK                   VS_FFI_FILEFLAGSMASK
#define VER_FILEOS                          VOS_NT_WINDOWS32
#define VER_FILEFLAGS                       (VER_PRERELEASE | \
                                             VER_DEBUG | \
                                             VER_PRIVATE)

//
// Company and Trademarks
//
#define VER_COMPANYNAME_STR                 \
    "ReactOS(R) Foundation"
#define VER_PRODUCTNAME_STR                 \
    "ReactOS(R) Operating System"
#define VER_LEGALTRADEMARKS_STR             \
    "ReactOS(R) is a registered trademark of the ReactOS Foundation."

#endif /* MPLAYER_NTVERP_H */
