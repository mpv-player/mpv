project(
    'phf_shared',
    'rust',
    version: '0.11.3',
    license: 'MIT',
)

lib = static_library(
    'phf_shared',
    'src/lib.rs',
    override_options: ['rust_std=2021', 'build.rust_std=2021'],
    rust_abi: 'rust',
    dependencies: dependency('siphasher', fallback: ['siphasher', 'siphasher_dep']),
)

phf_shared_dep = declare_dependency(
    link_with: [lib],
)
