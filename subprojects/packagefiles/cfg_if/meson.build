project(
    'cfg_if',
    'rust',
    version: '1.0.0',
    license: 'MIT OR Apache-2.0',
)

lib = static_library(
    'cfg_if',
    'src/lib.rs',
    override_options: ['rust_std=2021', 'build.rust_std=2021'],
    rust_abi: 'rust',
    native: true,
)

cfg_if_dep = declare_dependency(
    link_with: [lib],
)
