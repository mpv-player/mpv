#if HAVE_SOUNDCARD_H
#include <soundcard.h>
#endif

#if HAVE_SYS_SOUNDCARD_H
#include <sys/soundcard.h>
#endif

int main(int argc, char **argv) {
    return SNDCTL_DSP_SETFRAGMENT;
}
