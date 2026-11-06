project(
    'isolang',
    'rust',
    version: '2.4.0',
    license: 'Apache-2.0',
)

rust_args = [
  '-Aclippy::all',
]

lib = static_library(
    'isolang',
    'src/lib.rs',
    override_options: ['rust_std=2021', 'build.rust_std=2021'],
    rust_abi: 'rust',
    rust_args: rust_args,
    dependencies: dependency('phf', fallback: ['phf', 'phf_dep']),
)

isolang_dep = declare_dependency(
    link_with: [lib],
)
