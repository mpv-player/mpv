project(
    'tempfile',
    'rust',
    version: '3.17.1',
    license: 'MIT OR Apache-2.0',
)

rust_args = [
    '-A', 'unused_variables',
    '--cfg', 'feature="std"',
]

cfg_if_dep = dependency('cfg_if', fallback: ['cfg_if', 'cfg_if_dep'])
once_cell_dep = dependency('once_cell', fallback: ['once_cell', 'once_cell_dep'])
fastrand_dep = dependency('fastrand', fallback: ['fastrand', 'fastrand_dep'])

dependencies = [
    cfg_if_dep,
    fastrand_dep,
    once_cell_dep,
]

if host_machine.system() == 'windows'
    dependencies += dependency('windows_sys', fallback: ['windows_sys', 'windows_sys_dep'])
endif

lib = static_library(
    'tempfile',
    'src/lib.rs',
    rust_args: rust_args,
    override_options: ['rust_std=2021', 'build.rust_std=2021'],
    rust_abi: 'rust',
    native: true,
    dependencies: dependencies,
)

tempfile_dep = declare_dependency(
    link_with: [lib],
)
