#ifndef __WINE_WINDOWS_H
#define __WINE_WINDOWS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "shell.h"
#include "winreg.h"
#include "winnetwk.h"
#include "winver.h"
#include "lzexpand.h"
#include "shellapi.h"
#include "ole2.h"
#include "winnls.h"
#include "objbase.h"
#include "winspool.h"

#if 0
  Where does this belong? Nobody uses this stuff anyway.
typedef struct {
	BYTE i;  /* much more .... */
} KANJISTRUCT;
typedef KANJISTRUCT *LPKANJISTRUCT;
typedef KANJISTRUCT *NPKANJISTRUCT;
typedef KANJISTRUCT *PKANJISTRUCT;


#endif /* 0 */

#ifdef __cplusplus
}
#endif

#endif  /* __WINE_WINDOWS_H */
