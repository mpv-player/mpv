project(
    'siphasher',
    'rust',
    version: '1.0.1',
    license: 'MIT OR Apache-2.0',
)

lib = static_library(
    'siphasher',
    'src/lib.rs',
    override_options: ['rust_std=2021', 'build.rust_std=2021'],
    rust_abi: 'rust',
)

siphasher_dep = declare_dependency(
    link_with: [lib],
)
