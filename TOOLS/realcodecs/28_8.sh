rm 28_8.so.6.0
gcc -c 28_8.c -g
ld -shared -o 28_8.so.6.0 28_8.o -ldl -lc
 