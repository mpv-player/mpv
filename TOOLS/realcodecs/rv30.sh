rm rv30.so.6.0
gcc -c rv30.c -g
ld -shared -o rv30.so.6.0 rv30.o -ldl -lc
