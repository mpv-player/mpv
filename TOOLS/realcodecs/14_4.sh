rm 14_4.so.6.0
gcc -c 14_4.c -g
ld -shared -o 14_4.so.6.0 14_4.o -ldl -lc
 