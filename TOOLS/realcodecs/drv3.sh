rm drv3.so.6.0
gcc -c drv3.c -g &&
ld -shared -o drv3.so.6.0 drv3.o -ldl -lc
