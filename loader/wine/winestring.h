#ifndef __WINE_WINE_WINESTRING_H
#define __WINE_WINE_WINESTRING_H

#include "windef.h"

LPWSTR      WINAPI lstrcpyAtoW(LPWSTR,LPCSTR);
LPSTR       WINAPI lstrcpyWtoA(LPSTR,LPCWSTR);
LPWSTR      WINAPI lstrcpynAtoW(LPWSTR,LPCSTR,INT);
LPSTR       WINAPI lstrcpynWtoA(LPSTR,LPCWSTR,INT);

#define lstrncmpiA strncasecmp

#endif /* __WINE_WINE_WINESTRING_H */
