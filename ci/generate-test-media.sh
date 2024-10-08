#!/bin/sh
set -e

if [ "$#" -eq 0 ]; then
    exit 1
fi

cd $1

# Create core video, audio, and subtitle streams to construct files out of.
ffmpeg -y -f lavfi -i testsrc=duration=2:size=1280x720 video.mkv
ffmpeg -y -f lavfi -i sine=frequency=1000:duration=2 audio.flac
printf '1\n00:00:00,000 --> 00:00:01,000\nfoo\n2\n00:00:01,000 --> 00:00:02,000\nbar' > sub.srt

# Selecting subtitles with only 1 track has different logic so test separately.
ffmpeg -y -i video.mkv -i audio.flac -f srt -i sub.srt \
       -map 0:0 -map 1:0 -map 2:0 -c:v copy -c:a copy -c:s srt \
       -metadata:s:s:0 language=eng eng_no_default.mkv

ffmpeg -y -i video.mkv -i audio.flac -f srt -i sub.srt \
       -map 0:0 -map 1:0 -map 2:0 -c:v copy -c:a copy -c:s srt \
       -metadata:s:s:0 language=eng -disposition:s:s:0 default eng_default.mkv

ffmpeg -y -i video.mkv -i audio.flac -f srt -i sub.srt \
       -map 0:0 -map 1:0 -map 2:0 -c:v copy -c:a copy -c:s srt \
       -metadata:s:s:0 language=eng -disposition:s:s:0 forced eng_forced_no_audio.mkv

ffmpeg -y -i video.mkv -i audio.flac -f srt -i sub.srt \
       -map 0:0 -map 1:0 -map 2:0 -c:v copy -c:a copy -c:s srt \
       -metadata:s:a:0 language=eng -metadata:s:s:0 language=eng \
       -disposition:s:s:0 forced eng_forced_matching_audio.mkv

ffmpeg -y -i video.mkv -i audio.flac -f srt -i sub.srt \
       -map 0:0 -map 1:0 -map 2:0 -c:v copy -c:a copy -c:s srt \
       -metadata:s:s:0 language=eng -disposition:s:s:0 default+forced eng_default_forced.mkv

# Generate a file with multiple audio and subtitle languages.
ffmpeg -y -i video.mkv -i audio.flac -i audio.flac -i audio.flac -i audio.flac \
       -f srt -i sub.srt -i sub.srt -i sub.srt -i sub.srt \
       -map 0:0 -map 1:0 -map 2:0 -map 3:0 -map 4:0 -map 5:0 -map 6:0 -map 7:0 -map 8:0 \
       -metadata:s:a:0 language=eng -metadata:s:a:1 language=jpn -metadata:s:a:2 language=ger -metadata:s:a:3 language=pol \
       -metadata:s:s:0 language=eng -metadata:s:s:1 language=jpn -metadata:s:s:2 language=ger -metadata:s:s:3 language=pol \
       -disposition:s:s:0 default+forced -disposition:s:s:1 forced -disposition:s:s:2 default multilang.mkv
