rm drv2.so.6.0
gcc -c drv2.c -g &&
ld -shared -o drv2.so.6.0 drv2.o -ldl -lc
