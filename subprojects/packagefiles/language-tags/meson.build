project(
    'language-tags',
    'rust',
    version: '0.3.2',
    license: 'MIT OR Apache-2.0',
)

lib = static_library(
    'language_tags',
    'src/lib.rs',
    override_options: ['rust_std=2021', 'build.rust_std=2021'],
    rust_abi: 'rust',
)

language_tags_dep = declare_dependency(
    link_with: [lib],
)
