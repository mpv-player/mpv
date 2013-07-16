#include <CoreAudio/CoreAudio.h>
#include <AudioToolbox/AudioToolbox.h>
#include <AudioUnit/AudioUnit.h>

int main(int argc, char **argv)
{
    AudioComponentDescription desc = (AudioComponentDescription) {
        .componentType         = kAudioUnitType_Output,
        .componentSubType      = kAudioUnitSubType_DefaultOutput,
        .componentManufacturer = kAudioUnitManufacturer_Apple,
    };

    AudioComponentFindNext(NULL, &desc);
    return 0;
}
