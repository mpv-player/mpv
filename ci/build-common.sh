common_args="--werror \
-Dlibmpv=true \
-Dtests=true \
"

build_subrandr() {
    local target="$2"
    local prefix="$1"

    if [[ -n "$target" ]]; then
        target_arg="--target $target"
    else
        target_arg=""
    fi

    git clone --depth=1 https://github.com/afishhh/subrandr.git
    pushd subrandr
    cargo xtask install $target_arg --prefix "$prefix"
    popd
}

export CFLAGS="$CFLAGS -Wno-error=deprecated -Wno-error=deprecated-declarations -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3"
