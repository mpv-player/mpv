rm cook.so.6.0
gcc -c cook.c -g
ld -shared -o cook.so.6.0 cook.o -ldl -lc
 