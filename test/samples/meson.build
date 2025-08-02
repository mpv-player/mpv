# Create core video, audio, and subtitle streams to construct files out of.
video = custom_target('video.mkv',
    output: 'video.mkv',
    command: [ffmpeg, '-v', 'error', '-y', '-f', 'lavfi', '-i',
              'testsrc=duration=2:size=1280x720', '@OUTPUT@'],
)

audio = custom_target('audio.flac',
    output: 'audio.flac',
    command: [ffmpeg, '-v', 'error', '-y', '-f', 'lavfi', '-i',
              'sine=frequency=1000:duration=2', '@OUTPUT@'],
)

sub = join_paths(source_root, 'test', 'samples', 'sub.srt')

common_args = ['-v', 'error', '-y', '-i', video, '-i', audio,
               '-f', 'srt', '-i', sub, '-c', 'copy',
              '-metadata:s:s:0', 'language=eng']

samples = {
    'eng_default.mkv':
        [ffmpeg, common_args, '-disposition:s:0', 'default', '@OUTPUT@'],
    'eng_default_forced.mkv':
        [ffmpeg, common_args, '-disposition:s:0', 'default+forced', '@OUTPUT@'],
    'eng_forced_matching_audio.mkv':
        [ffmpeg, common_args, '-metadata:s:a:0', 'language=eng',
         '-disposition:s:0', 'forced', '@OUTPUT@'],
    'eng_forced_matching_audio_region.mkv':
        [ffmpeg, common_args, '-metadata:s:a:0', 'language=en-US',
         '-disposition:s:0', 'forced', '@OUTPUT@'],
    'eng_forced_no_matching_audio.mkv':
        [ffmpeg, common_args, '-disposition:s:0', 'forced', '@OUTPUT@'],
    'eng_forced_always_audio.mkv':
        [ffmpeg, common_args, '-metadata:s:a:0', 'language=ger',
         '-disposition:s:0', 'forced', '@OUTPUT@'],
    'eng_no_default.mkv':
        [ffmpeg, common_args, '@OUTPUT@'],
    'locale.mkv':
        [ffmpeg, '-v', 'error', '-y', '-i', video, '-i', audio, '-f', 'srt', '-i', sub, '-i', sub,
         '-c', 'copy', '-map', '0:0', '-map', '1:0', '-map', '2:0', '-map', '3:0', '-metadata:s:s:0',
         'language=ger', '-metadata:s:s:1', 'language=eng', '-disposition:s:0', 'default',
         '-disposition:s:1', 'default', '@OUTPUT@'],
    'multilang.mkv':
        [ffmpeg, '-v', 'error', '-y', '-i', video, '-i', audio, '-i', audio, '-i', audio, '-i', audio,
         '-f', 'srt', '-i', sub, '-i', sub, '-i', sub, '-i', sub, '-c', 'copy', '-map', '0:0',
         '-map', '1:0', '-map', '2:0', '-map', '3:0', '-map', '4:0', '-map', '5:0',
         '-map', '6:0', '-map', '7:0', '-map', '8:0', '-metadata:s:a:0', 'language=eng',
         '-metadata:s:a:1', 'language=jpn', '-metadata:s:a:2', 'language=ger', '-metadata:s:a:3',
         'language=pol', '-metadata:s:s:0', 'language=eng', '-metadata:s:s:1', 'language=jpn',
         '-metadata:s:s:2', 'language=ger', '-metadata:s:s:3', 'language=pol', '-disposition:s:0',
         'default+forced', '-disposition:s:1', 'forced', '-disposition:s:2', 'default', '@OUTPUT@'],
    'multilang2.mkv':
        [ffmpeg, '-v', 'error', '-y', '-i', video, '-i', audio, '-i', audio, '-f', 'srt', '-i', sub,
         '-i', sub, '-i', sub, '-i', sub, '-c', 'copy', '-map', '0:0', '-map', '1:0', '-map', '2:0',
         '-map', '3:0', '-map', '4:0', '-map', '5:0', '-map', '6:0', '-metadata:s:a:0', 'language=eng',
         '-metadata:s:a:1', 'language=jpn', '-metadata:s:s:0', 'language=jpn', '-metadata:s:s:1',
         'language=eng', '-metadata:s:s:2', 'language=eng', '-metadata:s:s:3', 'language=eng',
         '-disposition:s:0', 'default', '-disposition:s:1', 'forced', '-disposition:s:3', 'default',
         '@OUTPUT@'],
}

foreach name, cmd: samples
    target = custom_target(name,
        output: name,
        depends: [video, audio],
        command: cmd,
    )
    env = name == 'locale.mkv' ? 'LANG=C' : []
    testname = 'libmpv-test-' + name
    exe = executable(testname, '../libmpv_test_track_selection.c',
                     include_directories: incdir, dependencies: libmpv_dep)
    test(testname, exe, args: [name, target.full_path()],
         depends: target, env: env, suite: 'libmpv')
endforeach


sdh_default = join_paths(source_root, 'test', 'samples', 'sdh_default.srt')
sdh_mixed = join_paths(source_root, 'test', 'samples', 'sdh_mixed.srt')
sdh_all = join_paths(source_root, 'test', 'samples', 'sdh_all.srt')
sdh_mismatch = join_paths(source_root, 'test', 'samples', 'sdh_mismatch.srt')

sdh_samples = {
    'sdh_default':
        [ffmpeg, '-v', 'error', '-y', '-i', audio, '-f', 'srt',
        '-i', sdh_default, '-map', '0:0', '-map', '1:0', '-disposition:s:0',
        'default', '@OUTPUT@'],
    'sdh_mixed':
        [ffmpeg, '-v', 'error', '-y', '-i', audio, '-f', 'srt',
        '-i', sdh_mixed, '-map', '0:0', '-map', '1:0', '-disposition:s:0',
        'default', '@OUTPUT@'],
    'sdh_all':
        [ffmpeg, '-v', 'error', '-y', '-i', audio, '-f', 'srt',
        '-i', sdh_all, '-map', '0:0', '-map', '1:0', '-disposition:s:0',
        'default', '@OUTPUT@'],
    'sdh_mismatch':
        [ffmpeg, '-v', 'error', '-y', '-i', audio, '-f', 'srt',
        '-i', sdh_mismatch, '-map', '0:0', '-map', '1:0', '-disposition:s:0',
        'default', '@OUTPUT@'],
}

foreach name, cmd: sdh_samples
    target = custom_target(name,
        output: name + '.mkv',
        depends: [audio],
        command: cmd,
    )
    defaultname = 'libmpv-test-' + name
    exe = executable(defaultname, '../libmpv_test_sdh_filter.c',
                     include_directories: incdir, dependencies: libmpv_dep)
    hardername = 'libmpv-test-' + name + '_harder'
    test(defaultname, exe, args: [name, target.full_path()],
         depends: target, suite: 'libmpv')
    test(hardername, exe, args: [name, target.full_path(), 'yes'],
         depends: target, suite: 'libmpv')
endforeach
