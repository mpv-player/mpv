project(
    'language-tags',
    'rust',
    version: '0.3.2',
    license: 'MIT OR Apache-2.0',
)

rust_args = [
  '-Aclippy::all',
]

lib = static_library(
    'language_tags',
    'src/lib.rs',
    override_options: ['rust_std=2021', 'build.rust_std=2021'],
    rust_abi: 'rust',
    rust_args: rust_args,
)

language_tags_dep = declare_dependency(
    link_with: [lib],
)
