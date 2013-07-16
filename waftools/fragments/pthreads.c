#include <pthread.h>
static void *func(void *arg) { return arg; }
int main(void) {
    pthread_t tid;
#ifdef PTW32_STATIC_LIB
    pthread_win32_process_attach_np();
    pthread_win32_thread_attach_np();
#endif
    return pthread_create (&tid, 0, func, 0) != 0;
}
