common_args="--werror \
-Dlibmpv=true \
-Dtests=true \
"

export CFLAGS="$CFLAGS -Wno-error=deprecated -Wno-error=deprecated-declarations"
