int main(void) {
    int x;
    __asm__ volatile(
        "xor %0, %0"
        :"=b"(x)
        // just adding ebx to clobber list seems unreliable with some
        // compilers, e.g. Haiku's gcc 2.95
    );
    // and the above check does not work for OSX 64 bit...
    __asm__ volatile("":::"%ebx");
    return 0;
}
