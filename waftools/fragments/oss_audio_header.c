#if HAVE_SOUNDCARD_H
#include <soundcard.h>
#endif

#if HAVE_SYS_SOUNDCARD_H
#include <sys/soundcard.h>
#endif

#ifdef OPEN_SOUND_SYSTEM
int main(void) {{ return 0; }}
#else
#error Not the real thing
#endif
