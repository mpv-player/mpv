common_args="--werror \
-Dlibmpv=true \
-Dtests=true \
"

export CFLAGS="$CFLAGS -Wno-error=deprecated -Wno-error=deprecated-declarations -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3"
