#ifndef MPLAYER_OBJBASE_H
#define MPLAYER_OBJBASE_H

#ifndef STDCALL
#define STDCALL __attribute__((__stdcall__))
#endif

/* from objbase.h needed for ve_vfw.c */
typedef enum tagCOINIT {
    COINIT_APARTMENTTHREADED    = 0x2,
    COINIT_MULTITHREADED        = 0x0,
    COINIT_DISABLE_OLE1DDE      = 0x4,
    COINIT_SPEED_OVER_MEMORY    = 0x8
} COINIT;

HRESULT STDCALL CoInitialize(LPVOID pvReserved);
HRESULT STDCALL CoInitializeEx(LPVOID pvReserved, DWORD dwCoinit);
void STDCALL CoUninitialize(void);

#endif /* MPLAYER_OBJBASE_H */
