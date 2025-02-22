project(
    'fastrand',
    'rust',
    version: '2.3.0',
    license: 'MIT OR Apache-2.0',
)

rust_args = [
    '--cfg', 'feature="std"',
    '--cfg', 'feature="alloc"',
]

lib = static_library(
    'fastrand',
    'src/lib.rs',
    rust_args: rust_args,
    override_options: ['rust_std=2021', 'build.rust_std=2021'],
    rust_abi: 'rust',
    native: true,
)

fastrand_dep = declare_dependency(
    link_with: [lib],
)
