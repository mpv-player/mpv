project(
    'once_cell',
    'rust',
    version: '1.20.3',
    license: 'MIT OR Apache-2.0',
)

rust_args = [
    '--cfg', 'feature="std"',
]

lib = static_library(
    'once_cell',
    'src/lib.rs',
    rust_args: rust_args,
    override_options: ['rust_std=2021', 'build.rust_std=2021'],
    rust_abi: 'rust',
    native: true,
)

once_cell_dep = declare_dependency(
    link_with: [lib],
)
