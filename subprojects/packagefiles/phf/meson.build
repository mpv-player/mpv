project(
    'phf',
    'rust',
    version: '0.11.3',
    license: 'MIT',
)

lib = static_library(
    'phf',
    'src/lib.rs',
    override_options: ['rust_std=2021', 'build.rust_std=2021'],
    rust_abi: 'rust',
    dependencies: dependency('phf_shared', fallback: ['phf_shared', 'phf_shared_dep']),
)

phf_dep = declare_dependency(
    link_with: [lib],
)
