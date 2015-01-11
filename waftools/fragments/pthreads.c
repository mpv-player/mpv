#include <pthread.h>
static void *func(void *arg) { return arg; }
int main(void) {
    pthread_t tid;
    return pthread_create (&tid, 0, func, 0) != 0;
}
