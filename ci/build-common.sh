common_args="--werror \
-Dlibmpv=true \
-Dtests=true \
"

export CFLAGS="$CFLAGS -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3"

build_subrandr() {
    git clone --depth=1 https://github.com/afishhh/subrandr.git
    pushd subrandr
    cargo xtask install --prefix "$@"
    popd
}

build_details() {
    ./ci/build-details.py "$1"
    shift
    echo "### Packages"
    echo
    echo '```'
    "$@"
    echo '```'
}
