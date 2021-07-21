#include <stdatomic.h>
int main(int argc, char **argv) {
    atomic_int_least64_t test = ATOMIC_VAR_INIT(123);
    atomic_fetch_add(&test, 1);
    return 0;
}
