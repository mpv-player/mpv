rm sipr.so.6.0
gcc -c sipr.c -g
ld -shared -o sipr.so.6.0 sipr.o -ldl -lc
 