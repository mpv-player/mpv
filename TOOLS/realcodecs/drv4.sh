rm drv4.so.6.0
gcc -c drv4.c -g &&
ld -shared -o drv4.so.6.0 drv4.o -ldl -lc
