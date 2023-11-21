version_h = vcs_tag(
    command: ['git',
        '--git-dir=' + join_paths(source_root, '.git'),
        '--work-tree=' + source_root,
        'describe', '--always', '--tags', '--dirty'],
    input: 'version.h.in',
    output: 'version.h',
    replace_string: '@VERSION@',
)

sources += version_h
