version_h = vcs_tag(
    command: ['git', 'describe', '--always', '--tags', '--dirty'],
    input: 'version.h.in',
    output: 'version.h',
    replace_string: '@VERSION@',
)

sources += version_h
