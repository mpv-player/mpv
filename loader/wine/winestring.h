#ifndef WINE_WINESTRING_H
#define WINE_WINESTRING_H

#include "windef.h"

LPWSTR      WINAPI lstrcpyAtoW(LPWSTR,LPCSTR);
LPSTR       WINAPI lstrcpyWtoA(LPSTR,LPCWSTR);
LPWSTR      WINAPI lstrcpynAtoW(LPWSTR,LPCSTR,INT);
LPSTR       WINAPI lstrcpynWtoA(LPSTR,LPCWSTR,INT);

#define lstrncmpiA strncasecmp

#endif /* WINE_WINESTRING_H */
