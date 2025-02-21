project(
    'windows_targets',
    'rust',
    version: '0.53.0',
    license: 'MIT OR Apache-2.0',
)

lib = static_library(
    'windows_targets',
    'src/lib.rs',
    override_options: ['rust_std=2021', 'build.rust_std=2021'],
    rust_abi: 'rust',
    native: true,
)

windows_targets_dep = declare_dependency(
    link_with: [lib],
)
