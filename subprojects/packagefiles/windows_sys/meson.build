project(
    'windows_sys',
    'rust',
    version: '0.59.0',
    license: 'MIT OR Apache-2.0',
)

rust_args = [
    '--cfg', 'feature="Win32"',
    '--cfg', 'feature="Win32_Storage"',
    '--cfg', 'feature="Win32_Foundation"',
    '--cfg', 'feature="Win32_Storage_FileSystem"',
]

windows_targets_dep = dependency('windows_targets', fallback: ['windows_targets', 'windows_targets_dep'])

lib = static_library(
    'windows_sys',
    'src/lib.rs',
    rust_args: rust_args,
    override_options: ['rust_std=2021', 'build.rust_std=2021'],
    rust_abi: 'rust',
    native: true,
    dependencies: [windows_targets_dep],
)

windows_sys_dep = declare_dependency(
    link_with: [lib],
)
