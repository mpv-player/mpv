add_languages('rust', native: false)

libmpv_rs = static_library('libmpv_rs',
    'src/lib.rs',
    install: get_option('default_library') == 'static',
    rust_abi: 'c',
    dependencies: [
        dependency('isolang', fallback : ['isolang', 'isolang_dep']),
        dependency('language-tags', fallback : ['language-tags', 'language_tags_dep']),
    ],
)

dependencies += declare_dependency(link_with: libmpv_rs)
