#if defined(HAVE_SOUNDCARD_H) && HAVE_SOUNDCARD_H
#include <soundcard.h>
#endif

#if defined(HAVE_SYS_SOUNDCARD_H) && HAVE_SYS_SOUNDCARD_H
#include <sys/soundcard.h>
#endif

int main(int argc, char **argv) {
    return SNDCTL_DSP_SETFRAGMENT;
}
