#include <malloc.h>
#include <stdlib.h>
#include <process.h>
#include <wchar.h>
#include <windows.h>
#include <ks.h>
#include <audioclient.h>
#include <endpointvolume.h>
#include <mmdeviceapi.h>
#include <avrt.h>
    const GUID *check1[] = {
      &IID_IAudioClient,
      &IID_IAudioRenderClient,
      &IID_IAudioClient,
      &IID_IAudioEndpointVolume,
      &KSDATAFORMAT_SUBTYPE_PCM,
      &KSDATAFORMAT_SUBTYPE_IEEE_FLOAT,
      &KSDATAFORMAT_SPECIFIER_NONE,
    };
int main(void) {
    return 0;
}
