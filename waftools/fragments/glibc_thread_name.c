#define _GNU_SOURCE
#include <pthread.h>
int main(int argc, char **argv) {
    pthread_setname_np(pthread_self(), "ducks");
    return 0;
}
